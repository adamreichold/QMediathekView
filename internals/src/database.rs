use std::fs::{create_dir_all, remove_file};
use std::iter::from_fn;
use std::mem::replace;
use std::path::Path;
use std::sync::mpsc::Receiver;

use chrono::{NaiveDate, Timelike};
use rusqlite::{params, Connection, OpenFlags, OptionalExtension, Statement, NO_PARAMS};
use twoway::find_bytes;

use super::{
    compressor::{BackgroundCompressor, Decompressor},
    parser::Item,
    Error, Fallible,
};

const TEXT_BLOB_LEN: usize = 256 * 1024;
const URL_BLOB_LEN: usize = 512 * 1024;

pub const URL_SMALL: u32 = 0b01;
pub const URL_LARGE: u32 = 0b10;

pub fn open_connection(path: &Path) -> Fallible<Connection> {
    let conn = Connection::open_with_flags(
        path,
        OpenFlags::default() | OpenFlags::SQLITE_OPEN_PRIVATE_CACHE,
    )?;

    conn.pragma_update(None, "journal_mode", &"WAL")?;
    conn.pragma_update(None, "synchronous", &"NORMAL")?;

    Ok(conn)
}

pub fn create_schema(path: &Path) -> Fallible<(Connection, bool)> {
    create_dir_all(path.parent().unwrap())?;

    let mut conn = open_connection(path)?;

    let user_version: i32 = conn.pragma_query_value(None, "user_version", |row| row.get(0))?;

    if user_version == 8 {
        return Ok((conn, false));
    }

    remove_file(path)?;

    conn = open_connection(path)?;

    conn.execute_batch(
        r#"
BEGIN;

CREATE TABLE channels (
    id INTEGER PRIMARY KEY,
    channel TEXT NOT NULL,
    UNIQUE (channel)
);

CREATE TABLE topics (
    id INTEGER PRIMARY KEY,
    topic TEXT NOT NULL,
    channel_id INTEGER NOT NULL,
    UNIQUE (topic, channel_id)
);

CREATE INDEX topics_by_channel ON topics (channel_id);

CREATE TABLE shows (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    topic_id INTEGER NOT NULL,
    text_blob_id INTEGER NOT NULL,
    text_offset INTEGER NOT NULL,
    url_blob_id INTEGER NOT NULL,
    url_offset INTEGER NOT NULL,
    url_mask INTEGER NOT NULL,
    date INTEGER NOT NULL,
    time INTEGER NOT NULL,
    duration INTEGER NOT NULL
);

CREATE INDEX shows_by_topic ON shows (topic_id ASC, date DESC, time DESC);

CREATE VIRTUAL TABLE shows_by_title USING FTS5 (title, content='', detail=none);

CREATE TABLE blobs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    blob BLOB NOT NULL
);

INSERT INTO sqlite_sequence (name, seq) VALUES ('shows', 0);
INSERT INTO sqlite_sequence (name, seq) VALUES ('blobs', 0);

PRAGMA user_version = 8;

COMMIT;
"#,
    )?;

    Ok((conn, true))
}

pub fn full_update(conn: &Connection, items: &Receiver<Item>) -> Fallible<()> {
    conn.execute_batch(
        r#"
DELETE FROM blobs;
INSERT INTO shows_by_title (shows_by_title) VALUES ('delete-all');
DELETE FROM shows;
DELETE FROM topics;
DELETE FROM channels;
"#,
    )?;

    update(conn, items, |_, _, _| Ok(()))
}

pub fn partial_update(conn: &Connection, items: &Receiver<Item>) -> Fallible<()> {
    let max_show_id: i64 = conn.query_row(
        "SELECT seq FROM sqlite_sequence WHERE name = 'shows'",
        NO_PARAMS,
        |row| row.get(0),
    )?;

    let mut select_shows = conn.prepare(
        r#"
SELECT
    text_blob_id,
    text_offset,
    url_blob_id,
    url_offset,
    id
FROM shows
INDEXED BY shows_by_topic
WHERE topic_id = ?
AND id <= ?
ORDER BY id
"#,
    )?;

    let mut delete_show = conn.prepare("DELETE FROM shows WHERE id = ?")?;
    let mut delete_title = conn.prepare(
        "INSERT INTO shows_by_title (shows_by_title, rowid, title) VALUES ('delete', ?, ?)",
    )?;

    let mut text_fetcher = BlobFetcher::new();
    let mut url_fetcher = BlobFetcher::new();

    update(conn, items, |topic_id, title, url| {
        let mut rows = select_shows.query(params![topic_id, max_show_id])?;

        while let Some(row) = rows.next()? {
            let mut texts = text_fetcher.fetch(conn, row.get(0)?, row.get(1)?)?;
            if title.as_bytes() != texts.next().unwrap() {
                continue;
            }

            let mut urls = url_fetcher.fetch(conn, row.get(2)?, row.get(3)?)?;
            if url.as_bytes() != urls.next().unwrap() {
                continue;
            }

            let id: i64 = row.get(4)?;

            delete_show.execute(params![id])?;
            delete_title.execute(params![id, title])?;

            break;
        }

        Ok(())
    })
}

fn update<D: FnMut(i64, &str, &str) -> Fallible<()>>(
    conn: &Connection,
    items: &Receiver<Item>,
    mut deleter: D,
) -> Fallible<()> {
    let mut select_channel = conn.prepare("SELECT id FROM channels WHERE channel = ?")?;
    let mut insert_channel = conn.prepare("INSERT INTO channels (channel) VALUES (?)")?;
    let mut channel_id = 0;

    let mut select_topic =
        conn.prepare("SELECT id FROM topics WHERE topic = ? AND channel_id = ?")?;
    let mut insert_topic = conn.prepare("INSERT INTO topics (topic, channel_id) VALUES (?, ?)")?;
    let mut topic_id = 0;

    let mut insert_show = conn.prepare(
        r#"
INSERT INTO shows (
    topic_id,
    text_blob_id,
    text_offset,
    url_blob_id,
    url_offset,
    url_mask,
    date,
    time,
    duration
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
"#,
    )?;

    let mut insert_title =
        conn.prepare("INSERT INTO shows_by_title (rowid, title) VALUES (?,?)")?;

    let mut next_blob_id = {
        let mut update_blob_id =
            conn.prepare("UPDATE sqlite_sequence SET seq = seq + 1 WHERE name = 'blobs'")?;
        let mut select_blob_id =
            conn.prepare("SELECT seq FROM sqlite_sequence WHERE name = 'blobs'")?;

        move || {
            update_blob_id.execute(NO_PARAMS)?;
            select_blob_id.query_row(NO_PARAMS, |row| row.get::<_, i64>(0))
        }
    };

    let mut insert_blob = {
        let mut stmt = conn.prepare("INSERT INTO blobs (id, blob) VALUES (?, ?)")?;

        move |id: i64, blob: &[u8]| {
            stmt.execute(params![id, blob])?;

            Ok(())
        }
    };

    let mut text_compr = BackgroundCompressor::new();
    let mut text_blob_id = next_blob_id()?;

    let mut url_compr = BackgroundCompressor::new();
    let mut url_blob_id = next_blob_id()?;

    for item in items.iter() {
        if !item.topic.is_empty() {
            if !item.channel.is_empty() {
                channel_id = get_or_insert_channel(
                    conn,
                    &mut select_channel,
                    &mut insert_channel,
                    &item.channel,
                )?;
            }

            topic_id = get_or_insert_topic(
                conn,
                &mut select_topic,
                &mut insert_topic,
                &item.topic,
                channel_id,
            )?;
        }

        deleter(topic_id, &item.title, &item.url)?;

        insert_show_and_title(
            conn,
            topic_id,
            text_blob_id,
            &mut text_compr,
            url_blob_id,
            &mut url_compr,
            &mut insert_show,
            &mut insert_title,
            &item,
        )?;

        if text_compr.len() >= TEXT_BLOB_LEN {
            let blob_id = replace(&mut text_blob_id, next_blob_id()?);
            text_compr.rotate(blob_id, &mut insert_blob)?;
        }

        if url_compr.len() >= URL_BLOB_LEN {
            let blob_id = replace(&mut url_blob_id, next_blob_id()?);
            url_compr.rotate(blob_id, &mut insert_blob)?;
        }
    }

    text_compr.finish(text_blob_id, &mut insert_blob)?;
    url_compr.finish(url_blob_id, &mut insert_blob)?;

    Ok(())
}

#[allow(clippy::too_many_arguments)]
fn insert_show_and_title(
    conn: &Connection,
    topic_id: i64,
    text_blob_id: i64,
    text_compr: &mut BackgroundCompressor<i64>,
    url_blob_id: i64,
    url_compr: &mut BackgroundCompressor<i64>,
    insert_show: &mut Statement,
    insert_title: &mut Statement,
    item: &Item,
) -> Fallible<()> {
    let text_offset = text_compr.push(&item.title)?;
    text_compr.push(&item.description)?;

    let url_offset = url_compr.push(&item.url)?;
    let mut url_mask = 0;

    if let Some(url_small) = &item.url_small {
        url_compr.push(url_small)?;
        url_mask |= URL_SMALL;
    }

    if let Some(url_large) = &item.url_large {
        url_compr.push(url_large)?;
        url_mask |= URL_LARGE;
    }

    url_compr.push(&item.website)?;

    insert_show.execute(params![
        topic_id,
        text_blob_id,
        text_offset,
        url_blob_id,
        url_offset,
        url_mask,
        to_julian_day(item.date),
        item.time.num_seconds_from_midnight(),
        item.duration.num_seconds_from_midnight(),
    ])?;

    insert_title.execute(params![conn.last_insert_rowid(), item.title])?;

    Ok(())
}

fn get_or_insert_topic(
    conn: &Connection,
    select: &mut Statement,
    insert: &mut Statement,
    topic: &str,
    channel_id: i64,
) -> Fallible<i64> {
    let id: Option<i64> = select
        .query_row(params![topic, channel_id], |row| row.get(0))
        .optional()?;

    match id {
        Some(id) => Ok(id),
        None => {
            insert.execute(params![topic, channel_id])?;
            Ok(conn.last_insert_rowid())
        }
    }
}

fn get_or_insert_channel(
    conn: &Connection,
    select: &mut Statement,
    insert: &mut Statement,
    channel: &str,
) -> Fallible<i64> {
    let id: Option<i64> = select
        .query_row(params![channel], |row| row.get(0))
        .optional()?;

    match id {
        Some(id) => Ok(id),
        None => {
            insert.execute(params![channel])?;
            Ok(conn.last_insert_rowid())
        }
    }
}

pub struct BlobFetcher {
    blob_id: Option<i64>,
    decompr: Decompressor,
}

impl BlobFetcher {
    pub fn new() -> Self {
        Self {
            blob_id: None,
            decompr: Decompressor::new(),
        }
    }

    pub fn fetch(
        &mut self,
        conn: &Connection,
        blob_id: i64,
        offset: u32,
    ) -> Fallible<impl Iterator<Item = &[u8]>> {
        if self.blob_id != Some(blob_id) {
            let mut stmt = conn.prepare_cached("SELECT blob FROM blobs WHERE id = ?")?;
            let mut rows = stmt.query(params![blob_id])?;
            let row = rows
                .next()?
                .ok_or_else(|| Error::from(format!("No BLOB with ID {}", blob_id)))?;

            let blob = row.get_raw(0).as_blob()?;

            self.decompr.decompress(blob)?;
            self.blob_id = Some(blob_id);
        }

        let mut buf = &self.decompr.buf()[offset as usize..];

        Ok(from_fn(move || {
            find_bytes(buf, b"\0").map(|pos| {
                let val = &buf[..pos];
                buf = &buf[pos + 1..];
                val
            })
        }))
    }
}

fn to_julian_day(date: NaiveDate) -> i64 {
    date.signed_duration_since(NaiveDate::from_ymd(-4713, 11, 24))
        .num_days()
}
