use std::convert::TryInto;
use std::mem::replace;
use std::sync::mpsc::{channel, Receiver, Sender};

use rayon_core::spawn;
use zstd_safe::{
    compress_bound, compress_cctx, create_cctx, create_dctx, decompress_dctx, get_error_name,
    get_frame_content_size, CCtx, DCtx, CONTENTSIZE_ERROR, CONTENTSIZE_UNKNOWN,
};

use super::Fallible;

const COMPRESSION_LEVEL: i32 = 12;

pub struct BackgroundCompressor<T> {
    compr: Compressor,
    sender: Sender<Fallible<(T, Compressor)>>,
    receiver: Receiver<Fallible<(T, Compressor)>>,
}

impl<T: Send + 'static> BackgroundCompressor<T> {
    pub fn new() -> Self {
        let (sender, receiver) = channel();

        Self {
            compr: Compressor::new(),
            sender,
            receiver,
        }
    }

    pub fn push(&mut self, text: &str) -> Fallible<u32> {
        self.compr.push(text)
    }

    pub fn len(&self) -> usize {
        self.compr.buf.len()
    }

    pub fn rotate<F>(&mut self, tag: T, f: F) -> Fallible
    where
        F: FnOnce(T, &[u8]) -> Fallible,
    {
        let done = if let Ok(task) = self.receiver.try_recv() {
            let (tag, mut done) = task?;
            f(tag, &done.compr_buf)?;

            done.buf.clear();

            done
        } else {
            Compressor::new()
        };

        let mut todo = replace(&mut self.compr, done);

        spawn_task(self.sender.clone(), move || {
            todo.compress()?;
            Ok((tag, todo))
        });

        Ok(())
    }

    pub fn finish<F>(self, tag: T, mut f: F) -> Fallible
    where
        F: FnMut(T, &[u8]) -> Fallible,
    {
        if !self.compr.buf.is_empty() {
            let mut todo = self.compr;

            spawn_task(self.sender, move || {
                todo.compress()?;
                Ok((tag, todo))
            });
        } else {
            drop(self.sender);
        }

        for task in self.receiver.iter() {
            let (tag, done) = task?;
            f(tag, &done.compr_buf)?;
        }

        Ok(())
    }
}

fn spawn_task<T: Send + 'static, F: FnOnce() -> T + Send + 'static>(sender: Sender<T>, f: F) {
    spawn(move || {
        let _ = sender.send(f());
    });
}

struct Compressor {
    ctx: CCtx<'static>,
    compr_buf: Vec<u8>,
    buf: Vec<u8>,
}

impl Compressor {
    fn new() -> Self {
        Self {
            ctx: create_cctx(),
            compr_buf: Vec::new(),
            buf: Vec::new(),
        }
    }

    fn push(&mut self, text: &str) -> Fallible<u32> {
        if text.contains('\0') {
            return Err("Cannot compress text containing nul character".into());
        }

        let offset: u32 = self.buf.len().try_into().unwrap();
        self.buf.extend_from_slice(text.as_bytes());
        self.buf.push(b'\0');

        Ok(offset)
    }

    fn compress(&mut self) -> Fallible {
        self.compr_buf.resize(compress_bound(self.buf.len()), 0);

        match compress_cctx(
            &mut self.ctx,
            &mut self.compr_buf,
            &self.buf,
            COMPRESSION_LEVEL,
        ) {
            Ok(len) => {
                self.compr_buf.truncate(len);

                Ok(())
            }
            Err(err) => Err(format!("Zstd compression failed: {}", get_error_name(err)).into()),
        }
    }
}

pub struct Decompressor {
    ctx: DCtx<'static>,
    buf: Vec<u8>,
}

impl Decompressor {
    pub fn new() -> Self {
        Self {
            ctx: create_dctx(),
            buf: Vec::new(),
        }
    }

    pub fn buf(&self) -> &[u8] {
        &self.buf
    }

    pub fn decompress<'a>(&'a mut self, compr_buf: &[u8]) -> Fallible<&'a [u8]> {
        match get_frame_content_size(compr_buf) {
            CONTENTSIZE_ERROR | CONTENTSIZE_UNKNOWN => {
                return Err("Cannot determine uncompressed size".into())
            }
            len => self.buf.resize(len.try_into().unwrap(), 0),
        }

        match decompress_dctx(&mut self.ctx, &mut self.buf, compr_buf) {
            Ok(len) => {
                self.buf.truncate(len);

                Ok(&self.buf)
            }
            Err(err) => Err(format!("Zstd decompression failed: {}", get_error_name(err)).into()),
        }
    }
}
