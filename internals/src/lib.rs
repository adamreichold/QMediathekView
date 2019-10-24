#![allow(clippy::missing_safety_doc)]

mod compressor;
mod database;
mod parser;

use std::ffi::{CStr, CString, OsStr};
use std::os::{
    raw::{c_char, c_void},
    unix::ffi::OsStrExt,
};
use std::path::{Path, PathBuf};
use std::ptr::{null, null_mut};
use std::slice::from_raw_parts;
use std::str::from_utf8;
use std::sync::mpsc::{channel, Receiver};
use std::thread::spawn;

use attohttpc::get;
use failure::{ensure, format_err, Fallible};
use rusqlite::{Connection, NO_PARAMS};
use xz2::read::XzDecoder;

use self::database::{create_schema, full_update, open_connection, partial_update, BlobFetcher};
use self::parser::{parse, Item};

pub struct Internals {
    path: PathBuf,
    conn: Connection,
    text_fetcher: BlobFetcher,
    url_fetcher: BlobFetcher,
}

impl Internals {
    fn init<P: AsRef<Path>, N: FnOnce()>(path: P, needs_update: N) -> Fallible<Self> {
        let path = path.as_ref().join("database");
        let (conn, was_reset) = create_schema(&path)?;

        if was_reset {
            needs_update();
        }

        Ok(Self {
            path,
            conn,
            text_fetcher: BlobFetcher::new(),
            url_fetcher: BlobFetcher::new(),
        })
    }

    fn start_update<U, C>(&mut self, url: String, updater: U, completion: C)
    where
        U: 'static + FnOnce(&Connection, &Receiver<Item>) -> Fallible<()> + Send,
        C: 'static + FnOnce(Fallible<()>) + Send,
    {
        let path = self.path.clone();

        spawn(move || {
            completion(Self::update(&path, url, updater));
        });
    }

    fn update<U>(path: &Path, url: String, updater: U) -> Fallible<()>
    where
        U: FnOnce(&Connection, &Receiver<Item>) -> Fallible<()>,
    {
        let (sender, receiver) = channel();

        let parser = spawn(move || -> Fallible<()> {
            let response = get(url).follow_redirects(true).send()?;

            ensure!(
                response.is_success(),
                "Failed to download shows: {}",
                response.status()
            );

            let mut reader = XzDecoder::new(response.split().2);

            parse(&mut reader, &sender)?;

            Ok(())
        });

        let mut conn = open_connection(&path)?;

        let trans = conn.transaction()?;

        updater(&trans, &receiver)?;

        parser.join().unwrap()?;

        trans.execute("ANALYZE", NO_PARAMS)?;

        trans.commit()?;

        conn.execute_batch("PRAGMA wal_checkpoint(TRUNCATE);")?;

        Ok(())
    }

    fn channels<C: FnMut(StringData)>(&mut self, mut consumer: C) -> Fallible<()> {
        let mut stmt = self
            .conn
            .prepare("SELECT DISTINCT(channel) FROM channels")?;

        let mut rows = stmt.query(NO_PARAMS)?;

        while let Some(row) = rows.next()? {
            consumer(row.get_raw(0).as_str()?.into());
        }

        Ok(())
    }

    fn topics<C: FnMut(StringData)>(&mut self, channel: &str, mut consumer: C) -> Fallible<()> {
        let mut stmt = self.conn.prepare(
            r#"
SELECT DISTINCT(topic)
FROM channels, topics
WHERE channels.id = topics.channel_id
AND channels.channel LIKE ? || '%'
"#,
        )?;

        let mut rows = stmt.query(&[&channel])?;

        while let Some(row) = rows.next()? {
            consumer(row.get_raw(0).as_str()?.into());
        }

        Ok(())
    }

    fn query<C: FnMut(i64)>(
        &mut self,
        channel: &str,
        topic: &str,
        title: &str,
        mut consumer: C,
    ) -> Fallible<()> {
        let mut params = Vec::new();

        let channel_filter = if !channel.is_empty() {
            params.push(channel);
            "AND channels.channel LIKE ? || '%'"
        } else {
            ""
        };

        let topic_filter = if !topic.is_empty() {
            params.push(topic);
            "AND topics.topic LIKE ? || '%'"
        } else {
            ""
        };

        let title_filter = if !title.is_empty() {
            params.push(title);
            r#"AND shows_by_title MATCH ? || '*'"#
        } else {
            ""
        };

        let mut stmt = self.conn.prepare(&format!(
            r#"
SELECT shows.id
FROM channels, topics, shows, shows_by_title
WHERE channels.id = topics.channel_id
AND topics.id = shows.topic_id
AND shows.id = shows_by_title.rowid
{}
{}
{}
ORDER BY shows.topic_id ASC, shows.date DESC, shows.time DESC
"#,
            channel_filter, topic_filter, title_filter,
        ))?;

        let mut rows = stmt.query(&params)?;

        while let Some(row) = rows.next()? {
            consumer(row.get(0)?);
        }

        Ok(())
    }

    fn fetch<C: FnOnce(ShowData)>(&mut self, id: i64, consumer: C) -> Fallible<()> {
        let trans = self.conn.transaction()?;

        let mut stmt = trans.prepare(
            r#"
SELECT
    channels.channel,
    topics.topic,
    shows.text_blob_id,
    shows.url_blob_id,
    shows.title_offset,
    shows.date,
    shows.time,
    shows.duration,
    shows.description_offset,
    shows.website_offset,
    shows.url_offset,
    shows.url_small_offset,
    shows.url_large_offset
FROM channels, topics, shows, shows_by_title
WHERE channels.id = topics.channel_id
AND topics.id = shows.topic_id
AND shows.id = ?
"#,
        )?;

        let mut rows = stmt.query(&[&id])?;
        let row = rows
            .next()?
            .ok_or_else(|| format_err!("No show with ID {}", id))?;

        let channel = row.get_raw(0).as_str()?;
        let topic = row.get_raw(1).as_str()?;

        self.text_fetcher.fetch(&trans, row.get(2)?)?;
        self.url_fetcher.fetch(&trans, row.get(3)?)?;

        let title = self.text_fetcher.get(row.get::<_, i64>(4)? as _);

        let date = row.get(5)?;
        let time = row.get(6)?;
        let duration = row.get(7)?;

        let description = self.text_fetcher.get(row.get::<_, i64>(8)? as _);
        let website = self.url_fetcher.get(row.get::<_, i64>(9)? as _);

        let url = self.url_fetcher.get(row.get::<_, i64>(10)? as _);

        let url_small = if let Some(offset) = row.get::<_, Option<i64>>(11)? {
            Some(self.url_fetcher.get(offset as _))
        } else {
            None
        };

        let url_large = if let Some(offset) = row.get::<_, Option<i64>>(12)? {
            Some(self.url_fetcher.get(offset as _))
        } else {
            None
        };

        consumer(ShowData {
            channel: channel.into(),
            topic: topic.into(),
            title: title.into(),
            description: description.into(),
            website: website.into(),
            date,
            time,
            duration,
            url: url.into(),
            url_small: url_small.into(),
            url_large: url_large.into(),
        });

        Ok(())
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct NeedsUpdate {
    context: *mut c_void,
    action: unsafe extern "C" fn(context: *mut c_void),
}

#[no_mangle]
pub unsafe extern "C" fn internals_init(
    path: *const c_char,
    needs_update: NeedsUpdate,
) -> *mut Internals {
    let path = OsStr::from_bytes(CStr::from_ptr(path).to_bytes());

    match Internals::init(path, || (needs_update.action)(needs_update.context)) {
        Ok(internals) => Box::into_raw(Box::new(internals)),
        Err(err) => {
            eprintln!("Failed to initialize internals: {}", err);

            null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_drop(internals: *mut Internals) {
    Box::from_raw(internals);
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Completion {
    context: *mut c_void,
    action: unsafe extern "C" fn(context: *mut c_void, error: *const c_char),
}

unsafe impl Send for Completion {}

impl Completion {
    unsafe fn call(self, res: Fallible<()>) {
        let err = match res {
            Ok(()) => None,
            Err(err) => Some(CString::new(err.to_string()).unwrap()),
        };

        (self.action)(
            self.context,
            err.as_ref().map_or_else(null, |err| err.as_ptr()),
        );
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_full_update(
    internals: *mut Internals,
    url: *const c_char,
    completion: Completion,
) {
    let url = CStr::from_ptr(url).to_str().unwrap().to_owned();

    (*internals).start_update(url, full_update, move |res| completion.call(res));
}

#[no_mangle]
pub unsafe extern "C" fn internals_partial_update(
    internals: *mut Internals,
    url: *const c_char,
    completion: Completion,
) {
    let url = CStr::from_ptr(url).to_str().unwrap().to_owned();

    (*internals).start_update(url, partial_update, move |res| completion.call(res));
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct StringData {
    data: *const c_char,
    len: usize,
}

impl From<&[u8]> for StringData {
    fn from(val: &[u8]) -> Self {
        Self {
            data: val.as_ptr().cast(),
            len: val.len(),
        }
    }
}

impl From<Option<&[u8]>> for StringData {
    fn from(val: Option<&[u8]>) -> Self {
        match val {
            Some(val) => val.into(),
            None => Self {
                data: null(),
                len: 0,
            },
        }
    }
}

impl From<&str> for StringData {
    fn from(val: &str) -> Self {
        val.as_bytes().into()
    }
}

impl StringData {
    unsafe fn as_str(&self) -> &str {
        from_utf8(from_raw_parts(self.data.cast(), self.len)).unwrap()
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_channels(
    internals: *mut Internals,
    channels: *mut c_void,
    append: unsafe extern "C" fn(channels: *mut c_void, channel: StringData),
) {
    if let Err(err) = (*internals).channels(|channel| append(channels, channel)) {
        eprintln!("Failed to fetch channels: {}", err);
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_topics(
    internals: *mut Internals,
    channel: StringData,
    topics: *mut c_void,
    append: unsafe extern "C" fn(topics: *mut c_void, topic: StringData),
) {
    if let Err(err) = (*internals).topics(channel.as_str(), |topic| append(topics, topic)) {
        eprintln!("Failed to fetch topics: {}", err);
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_query(
    internals: *mut Internals,
    channel: StringData,
    topic: StringData,
    title: StringData,
    ids: *mut c_void,
    append: unsafe extern "C" fn(ids: *mut c_void, id: i64),
) {
    if let Err(err) = (*internals).query(channel.as_str(), topic.as_str(), title.as_str(), |id| {
        append(ids, id);
    }) {
        eprintln!("Failed to query shows: {}", err);
    }
}

#[repr(C)]
pub struct ShowData {
    channel: StringData,
    topic: StringData,
    title: StringData,
    date: i64,
    time: u32,
    duration: u32,
    description: StringData,
    website: StringData,
    url: StringData,
    url_small: StringData,
    url_large: StringData,
}

#[no_mangle]
pub unsafe extern "C" fn internals_fetch(
    internals: *mut Internals,
    id: i64,
    show: *mut c_void,
    fetch: unsafe extern "C" fn(show: *mut c_void, data: ShowData),
) {
    if let Err(err) = (*internals).fetch(id, |data| {
        fetch(show, data);
    }) {
        eprintln!("Failed to fetch show: {}", err);
    }
}
