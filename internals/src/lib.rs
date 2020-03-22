#![allow(improper_ctypes)]
#![allow(clippy::missing_safety_doc)]

mod compressor;
mod database;
mod parser;

use std::error::Error as StdError;
use std::ffi::{CStr, CString, OsStr};
use std::os::{
    raw::{c_char, c_void},
    unix::ffi::OsStrExt,
};
use std::path::{Path, PathBuf};
use std::ptr::{null, null_mut};
use std::slice::from_raw_parts;
use std::str::from_utf8;
use std::sync::mpsc::{sync_channel, Receiver};
use std::thread::spawn;

use curl::easy::Easy;
use rusqlite::{Connection, NO_PARAMS};

use self::database::{create_schema, full_update, open_connection, partial_update, BlobFetcher};
use self::parser::{Item, Parser};

pub type Error = Box<dyn StdError + Send + Sync>;
pub type Fallible<T = ()> = Result<T, Error>;

#[repr(C)]
pub enum SortColumn {
    Channel,
    Topic,
    Date,
    Time,
    Duration,
}

#[repr(C)]
pub enum SortOrder {
    Ascending,
    Descending,
}

pub struct Internals {
    path: PathBuf,
    conn: Connection,
    text_fetcher: BlobFetcher,
    url_fetcher: BlobFetcher,
}

impl Internals {
    fn init<P: AsRef<Path>>(path: P, needs_update: &mut bool) -> Fallible<Self> {
        let path = path.as_ref().join("database");
        let (conn, was_reset) = create_schema(&path)?;

        if was_reset {
            *needs_update = true;
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
        U: 'static + FnOnce(&Connection, &Receiver<Item>) -> Fallible + Send,
        C: 'static + FnOnce(Fallible) + Send,
    {
        let path = self.path.clone();

        spawn(move || {
            completion(Self::update(&path, url, updater));
        });
    }

    fn update<U>(path: &Path, url: String, updater: U) -> Fallible
    where
        U: FnOnce(&Connection, &Receiver<Item>) -> Fallible,
    {
        let (sender, receiver) = sync_channel(128);

        let parser = spawn(move || -> Fallible {
            let mut parser = Parser::new(sender);

            let mut handle = Easy::new();
            handle.url(&url)?;
            handle.follow_location(true)?;
            handle.fail_on_error(true)?;

            transfer(&mut handle, |data| parser.parse(data))?;

            parser.finish()?;

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

    fn channels<C: FnMut(StringData)>(&mut self, mut consumer: C) -> Fallible {
        let mut stmt = self
            .conn
            .prepare_cached("SELECT DISTINCT(channel) FROM channels")?;

        let mut rows = stmt.query(NO_PARAMS)?;

        while let Some(row) = rows.next()? {
            consumer(row.get_raw(0).as_str()?.into());
        }

        Ok(())
    }

    fn topics<C: FnMut(StringData)>(&mut self, channel: &str, mut consumer: C) -> Fallible {
        let mut stmt = self.conn.prepare_cached(
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
        sort_column: SortColumn,
        sort_order: SortOrder,
        mut consumer: C,
    ) -> Fallible {
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

        let order_by = match (sort_column, sort_order) {
            (SortColumn::Channel, SortOrder::Ascending) => {
                "shows.topic_id ASC, shows.date DESC, shows.time DESC"
            }
            (SortColumn::Channel, SortOrder::Descending) => {
                "shows.topic_id DESC, shows.date DESC, shows.time DESC"
            }
            (SortColumn::Topic, SortOrder::Ascending) => "topics.topic ASC",
            (SortColumn::Topic, SortOrder::Descending) => "topics.topic DESC",
            (SortColumn::Date, SortOrder::Ascending) => "shows.date ASC, shows.time ASC",
            (SortColumn::Date, SortOrder::Descending) => "shows.date DESC, shows.time DESC",
            (SortColumn::Time, SortOrder::Ascending) => "shows.time ASC",
            (SortColumn::Time, SortOrder::Descending) => "shows.time DESC",
            (SortColumn::Duration, SortOrder::Ascending) => "shows.duration ASC",
            (SortColumn::Duration, SortOrder::Descending) => "shows.duration DESC",
        };

        let mut stmt = self.conn.prepare_cached(&format!(
            r#"
SELECT shows.id
FROM channels, topics, shows, shows_by_title
WHERE channels.id = topics.channel_id
AND topics.id = shows.topic_id
AND shows.id = shows_by_title.rowid
{}
{}
{}
ORDER BY {}
"#,
            channel_filter, topic_filter, title_filter, order_by
        ))?;

        let mut rows = stmt.query(&params)?;

        while let Some(row) = rows.next()? {
            consumer(row.get(0)?);
        }

        Ok(())
    }

    fn fetch<C: FnOnce(ShowData)>(&mut self, id: i64, consumer: C) -> Fallible {
        let trans = self.conn.transaction()?;

        let mut stmt = trans.prepare_cached(
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
            .ok_or_else(|| Error::from(format!("No show with ID {}", id)))?;

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

fn transfer<F>(handle: &mut Easy, mut f: F) -> Fallible
where
    F: FnMut(&[u8]) -> Fallible,
{
    let mut write_error = None;
    let perform_result = {
        let mut transfer = handle.transfer();

        transfer.write_function(|data| match f(data) {
            Ok(()) => Ok(data.len()),
            Err(err) => {
                write_error = Some(err);
                Ok(0)
            }
        })?;

        transfer.perform()
    };

    if let Err(err) = perform_result {
        if err.is_write_error() {
            return Err(write_error.unwrap());
        }
    }

    Ok(())
}

extern "C" {
    fn append_integer(ids: *mut c_void, data: i64);
    fn append_string(strings: *mut c_void, data: StringData);
    fn fetch_show(show: *mut c_void, data: *const ShowData);
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct StringData {
    ptr: *const c_char,
    len: usize,
}

impl Default for StringData {
    fn default() -> Self {
        Self {
            ptr: null(),
            len: 0,
        }
    }
}

impl From<&[u8]> for StringData {
    fn from(val: &[u8]) -> Self {
        Self {
            ptr: val.as_ptr() as *const c_char,
            len: val.len(),
        }
    }
}

impl From<Option<&[u8]>> for StringData {
    fn from(val: Option<&[u8]>) -> Self {
        val.map(Into::into).unwrap_or_default()
    }
}

impl From<&str> for StringData {
    fn from(val: &str) -> Self {
        val.as_bytes().into()
    }
}

impl StringData {
    unsafe fn as_str(&self) -> &str {
        from_utf8(from_raw_parts(self.ptr as *const u8, self.len)).unwrap()
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
pub unsafe extern "C" fn internals_init(
    path: *const c_char,
    needs_update: *mut bool,
) -> *mut Internals {
    let path = OsStr::from_bytes(CStr::from_ptr(path).to_bytes());

    match Internals::init(path, &mut *needs_update) {
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
    unsafe fn call(self, res: Fallible) {
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

#[no_mangle]
pub unsafe extern "C" fn internals_channels(internals: *mut Internals, channels: *mut c_void) {
    if let Err(err) = (*internals).channels(|channel| append_string(channels, channel)) {
        eprintln!("Failed to fetch channels: {}", err);
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_topics(
    internals: *mut Internals,
    channel: StringData,
    topics: *mut c_void,
) {
    if let Err(err) = (*internals).topics(channel.as_str(), |topic| append_string(topics, topic)) {
        eprintln!("Failed to fetch topics: {}", err);
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_query(
    internals: *mut Internals,
    channel: StringData,
    topic: StringData,
    title: StringData,
    sort_column: SortColumn,
    sort_order: SortOrder,
    ids: *mut c_void,
) {
    if let Err(err) = (*internals).query(
        channel.as_str(),
        topic.as_str(),
        title.as_str(),
        sort_column,
        sort_order,
        |id| {
            append_integer(ids, id);
        },
    ) {
        eprintln!("Failed to query shows: {}", err);
    }
}

#[no_mangle]
pub unsafe extern "C" fn internals_fetch(internals: *mut Internals, id: i64, show: *mut c_void) {
    if let Err(err) = (*internals).fetch(id, |data| {
        fetch_show(show, &data);
    }) {
        eprintln!("Failed to fetch show: {}", err);
    }
}
