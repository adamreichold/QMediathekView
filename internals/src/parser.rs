use std::io::{Error as IoError, Read};
use std::num::ParseIntError;
use std::ptr::copy;
use std::sync::mpsc::Sender;

use chrono::{NaiveDate, NaiveTime};
use failure::Fail;
use serde::Deserialize;
use serde_json::{from_slice, from_str, value::RawValue, Error as JsonError};
use twoway::find_bytes;

pub struct Item {
    pub channel: String,
    pub topic: String,
    pub title: String,
    pub date: NaiveDate,
    pub time: NaiveTime,
    pub duration: NaiveTime,
    pub description: String,
    pub website: String,
    pub url: String,
    pub url_small: Option<String>,
    pub url_large: Option<String>,
}

pub fn parse<R: Read>(reader: &mut R, sender: &Sender<Item>) -> Result<(), Error> {
    let mut buf = Vec::new();

    loop {
        if let Some(parsed) = parse_header(&buf)? {
            truncate_buf(parsed, &mut buf);
            break;
        } else if !fill_buf(&mut buf, reader)? {
            return Err(Error::Header);
        }
    }

    loop {
        if let Some((parsed, item)) = parse_item(&buf)? {
            sender.send(item).map_err(|_| Error::Send)?;
            truncate_buf(parsed, &mut buf);
            continue;
        } else if !fill_buf(&mut buf, reader)? {
            let item = parse_last_item(&buf)?;
            sender.send(item).map_err(|_| Error::Send)?;
            break;
        }
    }

    Ok(())
}

fn fill_buf<R: Read>(buf: &mut Vec<u8>, reader: &mut R) -> Result<bool, Error> {
    let len = buf.len();
    buf.resize(len + 512, 0);
    let read = reader.read(&mut buf[len..])?;
    buf.truncate(len + read);

    Ok(read != 0)
}

fn truncate_buf(parsed: usize, buf: &mut Vec<u8>) {
    let len = buf.len().checked_sub(parsed).unwrap();

    unsafe {
        copy(buf.as_ptr().add(parsed), buf.as_mut_ptr(), len);
    }

    buf.truncate(len);
}

fn parse_header(input: &[u8]) -> Result<Option<usize>, Error> {
    const PREFIX: &[u8] = b"{\"Filmliste\":[";
    const SUFFIX: &[u8] = b"],\"X\":[";

    if input.len() < PREFIX.len() + SUFFIX.len() {
        return Ok(None);
    }

    if &input[..PREFIX.len()] != PREFIX {
        return Err(Error::Header);
    }

    let pos = match find_bytes(&input[PREFIX.len()..], SUFFIX) {
        Some(pos) => pos,
        None => return Ok(None),
    };

    Ok(Some(PREFIX.len() + pos + 2))
}

fn parse_item(input: &[u8]) -> Result<Option<(usize, Item)>, Error> {
    const PREFIX: &[u8] = b"\"X\":[";
    const SUFFIX: &[u8] = b"],\"X\":[";

    if input.len() < PREFIX.len() + SUFFIX.len() {
        return Ok(None);
    }

    if &input[..PREFIX.len()] != PREFIX {
        return Err(Error::Item);
    }

    let pos = match find_bytes(&input[PREFIX.len()..], SUFFIX) {
        Some(pos) => pos,
        None => return Ok(None),
    };

    let item = parse_fields(&input[PREFIX.len() - 1..][..pos + 2])?;

    Ok(Some((PREFIX.len() + pos + 2, item)))
}

fn parse_last_item(input: &[u8]) -> Result<Item, Error> {
    const PREFIX: &[u8] = b"\"X\":[";
    const SUFFIX: &[u8] = b"]}";

    if input.len() < PREFIX.len() + SUFFIX.len() {
        return Err(Error::LastItem);
    }

    if &input[..PREFIX.len()] != PREFIX {
        return Err(Error::LastItem);
    }

    if &input[input.len() - SUFFIX.len()..] != SUFFIX {
        return Err(Error::LastItem);
    }

    parse_fields(&input[PREFIX.len() - 1..=input.len() - SUFFIX.len()])
}

fn parse_fields(fields: &[u8]) -> Result<Item, Error> {
    #[derive(Deserialize)]
    struct Field<'a>(#[serde(borrow)] &'a RawValue);

    #[derive(Deserialize)]
    struct Fields<'a>(#[serde(borrow)] [Field<'a>; 20]);

    impl Fields<'_> {
        fn get(&self, index: usize) -> &str {
            self.0[index].0.get()
        }

        fn to_string(&self, index: usize) -> Result<String, Error> {
            from_str(self.get(index)).map_err(Error::from)
        }

        fn as_str(&self, index: usize) -> Result<&str, Error> {
            from_str(self.get(index)).map_err(Error::from)
        }
    }

    let fields: Fields = from_slice(fields)?;

    let channel = fields.to_string(0)?;
    let topic = fields.to_string(1)?;
    let title = fields.to_string(2)?;

    let date = parse_date(fields.as_str(3)?)?.unwrap_or_else(|| NaiveDate::from_ymd(1, 1, 1));
    let time = parse_time(fields.as_str(4)?)?.unwrap_or_else(|| NaiveTime::from_hms(0, 0, 0));
    let duration = parse_time(fields.as_str(5)?)?.unwrap_or_else(|| NaiveTime::from_hms(0, 0, 0));

    let description = fields.to_string(7)?;
    let website = fields.to_string(9)?;

    let url = fields.to_string(8)?;
    let url_small = parse_url_suffix(&url, fields.to_string(12)?)?;
    let url_large = parse_url_suffix(&url, fields.to_string(14)?)?;

    Ok(Item {
        channel,
        topic,
        title,
        date,
        time,
        duration,
        description,
        website,
        url,
        url_small,
        url_large,
    })
}

fn parse_date(field: &str) -> Result<Option<NaiveDate>, Error> {
    if field.is_empty() {
        return Ok(None);
    }

    let mut comps = field.split('.');

    let day = comps.next().ok_or_else(|| Error::Date)?.parse()?;
    let month = comps.next().ok_or_else(|| Error::Date)?.parse()?;
    let year = comps.next().ok_or_else(|| Error::Date)?.parse()?;

    Ok(Some(NaiveDate::from_ymd(year, month, day)))
}

fn parse_time(field: &str) -> Result<Option<NaiveTime>, Error> {
    if field.is_empty() {
        return Ok(None);
    }

    let mut comps = field.split(':');

    let hour = comps.next().ok_or_else(|| Error::Time)?.parse()?;
    let min = comps.next().ok_or_else(|| Error::Time)?.parse()?;
    let sec = comps.next().ok_or_else(|| Error::Time)?.parse()?;

    Ok(Some(NaiveTime::from_hms(hour, min, sec)))
}

fn parse_url_suffix(url: &str, mut field: String) -> Result<Option<String>, Error> {
    if field.is_empty() {
        return Ok(None);
    }

    if let Some(pos) = field.find('|') {
        let index = field[..pos].parse()?;
        let url = url.get(..index).ok_or_else(|| Error::UrlSuffix)?;

        field.replace_range(..=pos, url);
    } else {
        field.insert_str(0, url);
    }

    Ok(Some(field))
}

#[derive(Debug, Fail)]
pub enum Error {
    #[fail(display = "Failed to parse header")]
    Header,
    #[fail(display = "Failed to parse item")]
    Item,
    #[fail(display = "Failed to parse last item")]
    LastItem,
    #[fail(display = "Failed to parse date")]
    Date,
    #[fail(display = "Failed to parse time")]
    Time,
    #[fail(display = "Failed to parse URL suffix")]
    UrlSuffix,
    #[fail(display = "Failed to perform I/O: {}", _0)]
    Io(#[fail(cause)] IoError),
    #[fail(display = "Failed to parse JSON: {}", _0)]
    Json(#[fail(cause)] JsonError),
    #[fail(display = "Failed to parse integer: {}", _0)]
    ParseInt(#[fail(cause)] ParseIntError),
    #[fail(display = "Failed to send item")]
    Send,
}

impl From<IoError> for Error {
    fn from(err: IoError) -> Self {
        Error::Io(err)
    }
}

impl From<JsonError> for Error {
    fn from(err: JsonError) -> Self {
        Error::Json(err)
    }
}

impl From<ParseIntError> for Error {
    fn from(err: ParseIntError) -> Self {
        Error::ParseInt(err)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn url_suffix() {
        assert_eq!(None, parse_url_suffix("foo://bar", "".to_owned()).unwrap());

        assert_eq!(
            Some("foo://bar/qux".to_owned()),
            parse_url_suffix("foo://bar", "/qux".to_owned()).unwrap()
        );

        assert_eq!(
            Some("foo://bar/qux".to_owned()),
            parse_url_suffix("foo://bar/baz", "10|qux".to_owned()).unwrap()
        );
    }
}
