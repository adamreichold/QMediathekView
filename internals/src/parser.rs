use std::io::Read;
use std::sync::mpsc::SyncSender;

use chrono::{NaiveDate, NaiveTime};
use memchr::memmem::find;
use serde::Deserialize;
use serde_json::{from_slice, from_str, value::RawValue};

use super::{Error, Fallible};

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

pub fn parse<R: Read>(reader: &mut R, sender: SyncSender<Item>) -> Fallible {
    let mut buf = Vec::new();
    let mut pos = 0;

    loop {
        if let Some(parsed) = parse_header(&buf[pos..])? {
            pos += parsed;
            break;
        } else if !fill_buf(reader, &mut buf, &mut pos)? {
            return Err("Unexpected end of input".into());
        }
    }

    loop {
        if let Some((parsed, item)) = parse_item(&buf[pos..])? {
            pos += parsed;
            sender.send(item)?;
            continue;
        } else if !fill_buf(reader, &mut buf, &mut pos)? {
            let item = parse_last_item(&buf[pos..])?;
            sender.send(item)?;
            break;
        }
    }

    Ok(())
}

fn fill_buf<R: Read>(reader: &mut R, buf: &mut Vec<u8>, pos: &mut usize) -> Fallible<bool> {
    let len = buf.len() - *pos;
    buf.copy_within(*pos.., 0);
    *pos = 0;

    buf.resize(len + 32 * 1024, 0);
    let read = reader.read(&mut buf[len..])?;
    buf.truncate(len + read);

    Ok(read != 0)
}

fn parse_header(input: &[u8]) -> Fallible<Option<usize>> {
    const PREFIX: &[u8] = b"{\"Filmliste\":[";
    const SUFFIX: &[u8] = b"],\"X\":[";

    if input.len() < PREFIX.len() + SUFFIX.len() {
        return Ok(None);
    }

    if &input[..PREFIX.len()] != PREFIX {
        return Err("Malformed header".into());
    }

    let pos = match find(&input[PREFIX.len()..], SUFFIX) {
        Some(pos) => pos,
        None => return Ok(None),
    };

    Ok(Some(PREFIX.len() + pos + 2))
}

fn parse_item(input: &[u8]) -> Fallible<Option<(usize, Item)>> {
    const PREFIX: &[u8] = b"\"X\":[";
    const SUFFIX: &[u8] = b"],\"X\":[";

    if input.len() < PREFIX.len() + SUFFIX.len() {
        return Ok(None);
    }

    if &input[..PREFIX.len()] != PREFIX {
        return Err("Malformed item".into());
    }

    let pos = match find(&input[PREFIX.len()..], SUFFIX) {
        Some(pos) => pos,
        None => return Ok(None),
    };

    let item = parse_fields(&input[PREFIX.len() - 1..][..pos + 2])?;

    Ok(Some((PREFIX.len() + pos + 2, item)))
}

fn parse_last_item(input: &[u8]) -> Fallible<Item> {
    const PREFIX: &[u8] = b"\"X\":[";
    const SUFFIX: &[u8] = b"]}";

    if input.len() < PREFIX.len() + SUFFIX.len()
        || &input[..PREFIX.len()] != PREFIX
        || &input[input.len() - SUFFIX.len()..] != SUFFIX
    {
        return Err("Malformed last item".into());
    }

    parse_fields(&input[PREFIX.len() - 1..=input.len() - SUFFIX.len()])
}

fn parse_fields(fields: &[u8]) -> Fallible<Item> {
    #[derive(Deserialize)]
    struct Field<'a>(#[serde(borrow)] &'a RawValue);

    #[derive(Deserialize)]
    struct Fields<'a>(#[serde(borrow)] [Field<'a>; 20]);

    impl Fields<'_> {
        fn get(&self, index: usize) -> &str {
            self.0[index].0.get()
        }

        fn to_string(&self, index: usize) -> Fallible<String> {
            from_str(self.get(index)).map_err(Error::from)
        }

        fn as_str(&self, index: usize) -> Fallible<&str> {
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

fn parse_date(field: &str) -> Fallible<Option<NaiveDate>> {
    if field.is_empty() {
        return Ok(None);
    }

    let mut comps = field.split('.');

    let day = comps
        .next()
        .ok_or_else(|| Error::from("Missing day"))?
        .parse()?;
    let month = comps
        .next()
        .ok_or_else(|| Error::from("Missing month"))?
        .parse()?;
    let year = comps
        .next()
        .ok_or_else(|| Error::from("Missing year"))?
        .parse()?;

    Ok(Some(NaiveDate::from_ymd(year, month, day)))
}

fn parse_time(field: &str) -> Fallible<Option<NaiveTime>> {
    if field.is_empty() {
        return Ok(None);
    }

    let mut comps = field.split(':');

    let hour = comps
        .next()
        .ok_or_else(|| Error::from("Missing hours"))?
        .parse()?;
    let min = comps
        .next()
        .ok_or_else(|| Error::from("Missing minutes"))?
        .parse()?;
    let sec = comps
        .next()
        .ok_or_else(|| Error::from("Missing seconds"))?
        .parse()?;

    Ok(Some(NaiveTime::from_hms(hour, min, sec)))
}

fn parse_url_suffix(url: &str, mut field: String) -> Fallible<Option<String>> {
    if field.is_empty() {
        return Ok(None);
    }

    if let Some(pos) = field.find('|') {
        let index = field[..pos].parse()?;
        let url = url
            .get(..index)
            .ok_or_else(|| Error::from("Malformed URL suffix"))?;

        field.replace_range(..=pos, url);
    } else {
        field.insert_str(0, url);
    }

    Ok(Some(field))
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
