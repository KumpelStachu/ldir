# ldir
![MIT](https://img.shields.io/badge/license-MIT-blue) ![C](https://img.shields.io/badge/language-C-blue)

`ldir` is a simple command line tool written in C to list the contents of a directory. It is a simple implementation of the `ls` command in Unix.

## Installation

To install `ldir`, simply clone the repository and run the following commands:

```bash
$ git clone https://gh.kums.dev/ldir
$ cd ldir
$ make
$ sudo make install
$ # or
$ make install PREFIX=~/.local
```

## Usage

```bash
$ ldir --help
Usage: ldir [options] [path]
Options:
  -a,  --all            # show hidden files
  -l,  --long_format    # show long format
  -c,  --color          # colorize output
  -d,  --directories    # list directories only
  -f,  --no_sort        # do not sort
  -s,  --size_sort      # sort by size
  -t,  --type_sort      # sort by type
  -r,  --reverse        # reverse sort
  -h,  --human          # human readable sizes
  -H,  --help           # display this help and exit
```

## Examples

- List the contents of the current directory:
  ```bash
  $ ldir
  LICENSE  ldir     Makefile README   ldir.c
  ```

- List the contents of the current directory in long format, including hidden files, reverse sorted by size and human readable sizes:
  ```bash
  $ ldir -lar -s --human
  total 128
  -rw-r--r--   1 stanislaw  staff    14B Feb 21 23:15 .gitignore
  drwxr-xr-x   3 stanislaw  staff    96B Feb 21 23:41 .vscode
  drwxr-xr-x  10 stanislaw  staff   320B Feb 21 23:19 .git
  drwxr-xr-x  10 stanislaw  staff   320B Feb 22 01:25 .
  -rw-r--r--   1 stanislaw  staff   439B Feb 21 23:41 Makefile
  drwxr-xr-x@ 29 stanislaw  staff   928B Feb 20 23:24 ..
  -rw-r--r--   1 stanislaw  staff   1.0K Feb 21 23:35 LICENSE
  -rw-r--r--   1 stanislaw  staff   1.5K Feb 22 00:57 README
  -rw-r--r--   1 stanislaw  staff  10.0K Feb 22 01:26 ldir.c
  -rwxr-xr-x   1 stanislaw  staff  35.0K Feb 22 01:25 ldir
  ```

- List the contents of the current directory in long format, including hidden files and showing directories only:
  ```bash
  $ ldir -lad
  total 0
  drwxr-xr-x  10 stanislaw  staff  320 Feb 22 01:25 .
  drwxr-xr-x@ 29 stanislaw  staff  928 Feb 20 23:24 ..
  drwxr-xr-x  10 stanislaw  staff  320 Feb 21 23:19 .git
  drwxr-xr-x   3 stanislaw  staff   96 Feb 21 23:41 .vscode
  ```
