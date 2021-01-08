# QMediathekView [![Build Status](https://travis-ci.org/adamreichold/QMediathekView.svg?branch=master)](https://travis-ci.org/adamreichold/QMediathekView)

_QMediathekView_ is an alternative Qt-based front-end for the database maintained by the [MediathekView](https://mediathekview.de/) project. It has fewer features than the Java-based original, but should also consume less resources.

![Screenshot](https://user-images.githubusercontent.com/2480569/50730843-f1997c80-114e-11e9-8f25-2c137f453bbb.png)

The application is licensed under the GPL3+ and depends on the [Qt](https://www.qt.io/), the [SQLite](https://sqlite.org), the [LZMA](http://tukaani.org/xz/) and the [Zstd](https://facebook.github.io/zstd/) libraries. The default program used to play streams is the [VLC](https://www.videolan.org/vlc/) media player. The parser and database access layer are written in [Rust](https://www.rust-lang.org) and require at least version 1.39.0.
