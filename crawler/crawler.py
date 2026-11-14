#!/usr/bin/env python3
"""
XPDevs Gateway Web Crawler v3.1.0
================================

A memory-bounded, single-file, dependency-free web crawler with a full
terminal UI, producing a valid JSON array compatible with the XPDevs
Gateway search engine.

v3.1.0 changes
-------------
- FIX: Frontier._seen no longer leaks memory (discard on pop).
- FIX: SSRF via HTTP redirect is now blocked by SafeRedirectHandler.
- FIX: Non-HTML (PDF/zip/etc.) responses are no longer written as empty records.
- FIX: Resume now restores _domain_pages and stats counters from the checkpoint.
- FIX: HTTPResponse attributes are read before close().
- FIX: signal.signal() no longer raises when constructed off the main thread.
- FIX: gzip/deflate decompression no longer drops the tail (flush() + unconsumed_tail).
- FIX: NO_TUI=0 (and "false"/"no") no longer disables the TUI.
- FIX: Sitemap and feed URLs now honour --same-host and robots.txt.
- FIX: TUI key polling continues during the per-host polite wait.
- FIX: <title> length limit is now by character, not by HTMLParser chunk.
- NEW: ntfy.sh push notifications via --ntfy-topic (no token needed).
- NEW: --ntfy-events, --ntfy-progress-every.

OUTPUT FORMAT
-------------
Always a single valid JSON array in a .json file (never JSONL/NDJSON).
Atomic write: <path>.part -> fsync -> os.replace(<path>).
Empty crawl -> [].
"""


import argparse
import base64
import codecs
import gzip
import hashlib
import heapq
import ipaddress
import json
import math
import os
import re
import select
import shutil
import signal
import socket
import sqlite3
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import urllib.robotparser
import xml.etree.ElementTree as ET
import zlib
from collections import Counter, OrderedDict, deque
from html.parser import HTMLParser

try:
    import termios
    import tty
    HAS_TERMIOS = True
except ImportError:
    HAS_TERMIOS = False

try:
    import msvcrt
    HAS_MSVCRT = True
except ImportError:
    HAS_MSVCRT = False

VERSION = "3.1.0"

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
DEFAULT_USER_AGENT     = "GatewayCrawler/3.1 (+https://github.com/XPDevs/gateway)"
DEFAULT_DELAY          = 1.0
DEFAULT_TIMEOUT        = 10.0
DEFAULT_OUTPUT         = "gateway_index.json"
DEFAULT_MAX_QUEUE      = 20_000
DEFAULT_BLOOM_CAP      = 1_000_000
DEFAULT_BLOOM_FP       = 0.01
DEFAULT_PAGE_BYTES     = 2 * 1024 * 1024
DEFAULT_CHUNK          = 64 * 1024
DEFAULT_ROBOTS_CAP     = 200
DEFAULT_ROBOTS_BYTES   = 100 * 1024
DEFAULT_RATELIM_CAP    = 2_000
DEFAULT_MAX_RETRIES    = 3
DEFAULT_MAX_DEPTH      = 5
DEFAULT_MAX_PER_DOM    = 5_000
DEFAULT_MAX_LINKS      = 200
DEFAULT_CHECKPOINT     = ".gateway_checkpoint.json"

# ntfy defaults
DEFAULT_NTFY_SERVER    = "https://ntfy.sh"
DEFAULT_NTFY_EVENTS    = "started,finished"
DEFAULT_NTFY_TIMEOUT   = 5.0

STOPWORDS = frozenset("""
a about above after again against all am an and any are as at be because
been before being below between both but by could did do does doing down
during each few for from further had has have having he her here hers
herself him himself his how i if in into is it its itself just me more
most my myself no nor not of off on once only or other our ours ourselves
out over own same she should so some such than that the their theirs them
themselves then there these they this those through to too under until up
very was we were what when where which while who whom why will with you
your yours yourself yourselves
""".split())


# ---------------------------------------------------------------------------
# Util: Unicode detection for TUI
# ---------------------------------------------------------------------------
def _stdout_supports_unicode():
    try:
        enc = sys.stdout.encoding or "ascii"
        "█─│┌┐└┘├┤┬┴┼✓✗·".encode(enc)
        return True
    except Exception:
        return False


def _env_flag(name, default=False):
    """FIX (#9): correct boolean parsing for env flags like NO_TUI=0."""
    v = os.environ.get(name)
    if v is None:
        return default
    return v.strip().lower() in ("1", "true", "yes", "on", "y", "t")


# ---------------------------------------------------------------------------
# Logger
# ---------------------------------------------------------------------------
class Logger:
    def __init__(self, verbose=True, quiet=False, fmt="text", tui=None,
                 log_file=None):
        self.verbose = verbose
        self.quiet = quiet
        self.fmt = fmt
        self.tui = tui
        self._t0 = time.time()
        self._file = None
        if log_file:
            self._file = open(log_file, "w", encoding="utf-8", buffering=1)
            self._file.write(f"Gateway crawler debug log started: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")

    def _emit(self, level, msg, **fields):
        elapsed = round(time.time() - self._t0, 3)
        rec = {"t": elapsed, "level": level, "msg": msg}
        if fields:
            rec.update(fields)
        if self._file:
            self._file.write(json.dumps(rec, ensure_ascii=False, default=str) + "\n")
            self._file.flush()
        if self.quiet and level in ("INFO", "DEBUG"):
            return
        if level == "DEBUG" and not self.verbose:
            return
        if self.tui and self.tui.enabled:
            self.tui.add_log(level, msg)
            return
        if self.fmt == "json":
            print(json.dumps(rec, ensure_ascii=False, default=str), file=sys.stderr, flush=True)
        else:
            print(f"[{level}] {msg}", file=sys.stderr, flush=True)

    def info(self, msg, **f):  self._emit("INFO", msg, **f)
    def warn(self, msg, **f):  self._emit("WARN", msg, **f)
    def error(self, msg, **f): self._emit("ERROR", msg, **f)
    def debug(self, msg, **f): self._emit("DEBUG", msg, **f)

    def close(self):
        if self._file:
            try:
                self._file.write(json.dumps({"t": round(time.time()-self._t0, 3),
                                             "level": "INFO", "msg": "debug log closed"}) + "\n")
                self._file.close()
            finally:
                self._file = None


# ---------------------------------------------------------------------------
# RSS
# ---------------------------------------------------------------------------
def get_peak_rss_mb():
    try:
        import resource
    except ImportError:
        return None
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return (rss / (1024 * 1024)) if sys.platform == "darwin" else (rss / 1024)


def get_current_rss_mb():
    try:
        with open("/proc/self/status", "r") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) / 1024.0
    except Exception:
        pass
    return get_peak_rss_mb()


# ---------------------------------------------------------------------------
# SSRF protection
# ---------------------------------------------------------------------------
def _ip_is_forbidden(ip):
    try:
        addr = ipaddress.ip_address(ip)
    except ValueError:
        return True
    return (addr.is_private or addr.is_loopback or addr.is_link_local
            or addr.is_reserved or addr.is_multicast or addr.is_unspecified)


_METADATA_HOSTS = {"169.254.169.254", "metadata.google.internal",
                   "metadata.goog", "100.100.100.200"}


def is_safe_to_fetch(url):
    try:
        p = urllib.parse.urlparse(url)
    except Exception:
        return False, "unparseable url"
    if p.scheme not in ("http", "https"):
        return False, f"scheme {p.scheme!r} not allowed"
    host = p.hostname
    if not host:
        return False, "no host"
    host_l = host.lower()
    if host_l in _METADATA_HOSTS:
        return False, "metadata endpoint"
    if host_l in ("localhost", "localhost.localdomain") or host_l.endswith(".localhost"):
        return False, "localhost"
    try:
        infos = socket.getaddrinfo(host, None)
    except Exception as e:
        return False, f"dns failure: {e}"
    for fam, _, _, _, sockaddr in infos:
        if _ip_is_forbidden(sockaddr[0]):
            return False, f"forbidden ip {sockaddr[0]}"
    return True, "ok"


# ---------------------------------------------------------------------------
# FIX (#2): SSRF-safe redirect handler + shared opener
#
# urllib.request.urlopen() follows redirects automatically. The initial URL
# is validated by is_safe_to_fetch(), but a 30x Location: header could point
# to http://169.254.169.254/ (cloud metadata) or http://127.0.0.1:6379/
# (Redis), bypassing our check entirely. This handler re-validates every hop.
#
# Note: DNS rebinding (a hostname that resolves to a public IP on the first
# check but a private one on the second) is still possible. Fully closing
# that requires pinning the resolved IP and connecting to it directly, which
# is intentionally out of scope here.
# ---------------------------------------------------------------------------
class SafeRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        safe, reason = is_safe_to_fetch(newurl)
        if not safe:
            raise urllib.error.HTTPError(
                newurl, code, f"redirect blocked: {reason}", headers, fp)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


_SAFE_OPENER = None
_SAFE_OPENER_LOCK = threading.Lock()


def _get_safe_opener():
    global _SAFE_OPENER
    if _SAFE_OPENER is None:
        with _SAFE_OPENER_LOCK:
            if _SAFE_OPENER is None:
                _SAFE_OPENER = urllib.request.build_opener(SafeRedirectHandler())
    return _SAFE_OPENER


def safe_urlopen(req, timeout):
    """Drop-in replacement for urllib.request.urlopen() that rejects
    redirects to SSRF-unsafe destinations."""
    return _get_safe_opener().open(req, timeout=timeout)


# ---------------------------------------------------------------------------
# URL utils
# ---------------------------------------------------------------------------
def normalize_url(url, base=None):
    if base:
        url = urllib.parse.urljoin(base, url)
    if "://" not in url:
        url = "https://" + url
    try:
        p = urllib.parse.urlparse(url)
    except Exception:
        return None
    if p.scheme not in ("http", "https"):
        return None
    p = p._replace(fragment="")
    path = p.path.rstrip("/") or "/"
    p = p._replace(path=path)
    return urllib.parse.urlunparse(p)


def get_domain(url):
    try:
        d = urllib.parse.urlparse(url).netloc.lower()
    except Exception:
        return ""
    if d.startswith("www."):
        d = d[4:]
    return d


def is_same_host(u1, u2):
    return get_domain(u1) == get_domain(u2)


def content_hash(text):
    return hashlib.sha256(text.encode("utf-8", "replace")).hexdigest()


def stable_id(url):
    return hashlib.sha1(url.encode("utf-8", "replace")).hexdigest()[:16]


# ---------------------------------------------------------------------------
# Bloom
# ---------------------------------------------------------------------------
class BloomFilter:
    def __init__(self, capacity, error_rate=DEFAULT_BLOOM_FP):
        m = int(-capacity * math.log(error_rate) / (math.log(2) ** 2))
        self.num_bits = ((m + 7) // 8) * 8
        self.num_hashes = max(1, int(round(self.num_bits / capacity * math.log(2))))
        self._bits = bytearray(self.num_bits // 8)
        self.capacity = capacity
        self.error_rate = error_rate
        self.added = 0

    def _positions(self, item):
        d = hashlib.md5(item.encode("utf-8", "replace")).digest()
        h1 = int.from_bytes(d[:8], "little")
        h2 = int.from_bytes(d[8:], "little") or 1
        for i in range(self.num_hashes):
            yield (h1 + i * h2) % self.num_bits

    def add(self, item):
        for pos in self._positions(item):
            self._bits[pos >> 3] |= 1 << (pos & 7)
        self.added += 1

    def __contains__(self, item):
        for pos in self._positions(item):
            if not (self._bits[pos >> 3] & (1 << (pos & 7))):
                return False
        return True

    def saturation(self):
        set_bits = sum(bin(b).count("1") for b in self._bits)
        return set_bits / self.num_bits if self.num_bits else 0.0

    def bytes_used(self):
        return len(self._bits)

    def to_dict(self):
        return {"num_bits": self.num_bits, "num_hashes": self.num_hashes,
                "capacity": self.capacity, "error_rate": self.error_rate,
                "added": self.added,
                "bits_b64": base64.b64encode(bytes(self._bits)).decode("ascii")}

    @classmethod
    def from_dict(cls, d):
        obj = cls(d["capacity"], d["error_rate"])
        obj.num_bits = d["num_bits"]
        obj.num_hashes = d["num_hashes"]
        obj.added = d.get("added", 0)
        obj._bits = bytearray(base64.b64decode(d["bits_b64"]))
        return obj


# ---------------------------------------------------------------------------
# Bounded LRU dict
# ---------------------------------------------------------------------------
class BoundedDict(OrderedDict):
    def __init__(self, maxsize):
        super().__init__()
        self.maxsize = maxsize

    def __setitem__(self, key, value):
        if key in self:
            super().__setitem__(key, value)
            self.move_to_end(key)
            return
        if len(self) >= self.maxsize:
            self.popitem(last=False)
        super().__setitem__(key, value)


# ---------------------------------------------------------------------------
# Frontier
# ---------------------------------------------------------------------------
class Frontier:
    def __init__(self, maxsize):
        self.maxsize = maxsize
        self._heap = []
        self._seen = set()
        self._counter = 0
        self.dropped = 0

    def __len__(self):
        return len(self._heap)

    def __bool__(self):
        return bool(self._heap)

    def push(self, url, depth, base_url=None):
        if url in self._seen:
            return False
        if len(self._heap) >= self.maxsize:
            self.dropped += 1
            return False
        self._counter += 1
        prio = depth * 10
        if base_url and is_same_host(url, base_url):
            prio -= 3
        heapq.heappush(self._heap, (prio, self._counter, url, depth, base_url))
        self._seen.add(url)
        return True

    def pop(self):
        if not self._heap:
            return None
        _, _, url, depth, base_url = heapq.heappop(self._heap)
        # FIX (#1): discard from _seen so the set doesn't grow unboundedly.
        # Dedup is still enforced by the caller's bloom filter (self.seen)
        # which is what actually bounds memory.
        self._seen.discard(url)
        return url, depth, base_url

    def snapshot(self):
        """Non-destructive view of remaining frontier (for checkpoint)."""
        return [(url, depth, base) for (_p, _c, url, depth, base) in self._heap]


# ---------------------------------------------------------------------------
# Keyboard
# ---------------------------------------------------------------------------
class KeyReader:
    def __init__(self):
        self._is_windows = sys.platform == "win32"
        self._old_settings = None
        self._ok = False
        if not self._is_windows and HAS_TERMIOS and sys.stdin.isatty():
            try:
                self._old_settings = termios.tcgetattr(sys.stdin.fileno())
                tty.setcbreak(sys.stdin.fileno())
                self._ok = True
            except Exception:
                self._old_settings = None
        elif self._is_windows and HAS_MSVCRT:
            self._ok = True

    def poll(self):
        if not self._ok:
            return None
        if self._is_windows:
            if msvcrt.kbhit():
                try:
                    ch = msvcrt.getwch()
                    return ch
                except Exception:
                    return None
            return None
        try:
            r, _, _ = select.select([sys.stdin], [], [], 0)
            if r:
                return sys.stdin.read(1)
        except Exception:
            pass
        return None

    def close(self):
        if (not self._is_windows) and self._old_settings and HAS_TERMIOS:
            try:
                termios.tcsetattr(sys.stdin.fileno(),
                                  termios.TCSADRAIN, self._old_settings)
            except Exception:
                pass


# ---------------------------------------------------------------------------
# TUI (pure ANSI)
# ---------------------------------------------------------------------------
class TUI:
    REFRESH_DEFAULT = 0.5

    def __init__(self, refresh_rate=REFRESH_DEFAULT, force=False):
        # FIX (#9): correct boolean parsing for NO_TUI env var.
        no_tui = _env_flag("NO_TUI", default=False)
        self.enabled = sys.stdout.isatty() and not no_tui
        self.enabled = self.enabled or force
        self.unicode = _stdout_supports_unicode()
        self.refresh_rate = refresh_rate
        self.log_lines = deque(maxlen=300)
        self.key_reader = None
        self._state = {}
        self._start = time.time()
        self._last_render = 0.0
        self._info_mode = False
        self._current = ""
        self._write_buf = sys.stdout

    def enter(self):
        if not self.enabled:
            return
        self.key_reader = KeyReader()
        self._out("\033[?1049h")
        self._out("\033[?25l")
        self._out("\033[2J\033[H")
        self._flush()

    def exit(self):
        if not self.enabled:
            return
        if self.key_reader:
            self.key_reader.close()
        self._out("\033[?25h")
        self._out("\033[?1049l")
        self._flush()

    def _out(self, s):
        try:
            self._write_buf.write(s)
        except Exception:
            pass

    def _flush(self):
        try:
            self._write_buf.flush()
        except Exception:
            pass

    def color(self, fg=None, bold=False, dim=False):
        parts = []
        if bold: parts.append("1")
        if dim:  parts.append("2")
        if fg is not None: parts.append(str(fg))
        if not parts:
            return ""
        return "\033[" + ";".join(parts) + "m"

    def reset(self):
        return "\033[0m"

    def add_log(self, level, msg):
        msg = re.sub(r"[\x00-\x08\x0b-\x1f\x7f]", "", str(msg))
        self.log_lines.append((level, msg, time.time()))

    def clear_log(self):
        self.log_lines.clear()

    def toggle_info(self):
        self._info_mode = not self._info_mode

    def poll_key(self):
        if not self.key_reader:
            return None
        return self.key_reader.poll()

    def update_state(self, state):
        self._state = state

    def set_current(self, url):
        self._current = url or ""

    def _get_size(self):
        try:
            sz = shutil.get_terminal_size((100, 30))
            return sz.columns, sz.lines
        except Exception:
            return 100, 30

    def _vlen(self, s):
        return len(re.sub(r"\033\[[0-9;]*m", "", s))

    def _trunc(self, s, n):
        if n <= 0:
            return ""
        plain = re.sub(r"\033\[[0-9;]*m", "", s)
        if len(plain) <= n:
            return s
        return plain[: max(0, n - 1)] + ("…" if self.unicode else ".")

    def _level_icon(self, level):
        if level == "INFO":  return self.color(92, bold=True) + ("✓" if self.unicode else "+") + self.reset()
        if level == "WARN":  return self.color(93, bold=True) + ("!" if not self.unicode else "⚠") + self.reset()
        if level == "ERROR": return self.color(91, bold=True) + ("✗" if self.unicode else "x") + self.reset()
        return self.color(90) + ("·" if self.unicode else ".") + self.reset()

    def _bar(self, pct, width):
        pct = max(0.0, min(1.0, pct))
        filled = int(width * pct)
        empty = max(0, width - filled)
        if self.unicode:
            body = (self.color(92) + "█" * filled
                    + self.color(90) + "░" * empty + self.reset())
        else:
            body = (self.color(92) + "#" * filled
                    + self.color(90) + "-" * empty + self.reset())
        return "[" + body + "]"

    def render(self):
        if not self.enabled:
            return
        now = time.time()
        if now - self._last_render < self.refresh_rate:
            return
        self._last_render = now
        cols, rows = self._get_size()
        if cols < 60 or rows < 16:
            self._out("\033[H\033[K")
            self._out(f"Terminal too small ({cols}x{rows}); need 60x16")
            self._flush()
            return
        try:
            frame = self._build_frame(cols, rows)
        except Exception:
            return
        out = ["\033[H"]
        for line in frame:
            out.append(line)
            out.append("\033[K\r\n")
        for _ in range(max(0, rows - len(frame))):
            out.append("\033[K\r\n")
        self._out("".join(out))
        self._flush()

    def _build_frame(self, cols, rows):
        s = self._state
        status = s.get("status", "RUNNING")
        elapsed = s.get("elapsed", 0)
        pages = s.get("pages", 0)
        max_pages = s.get("max_pages")
        queue = s.get("queue", 0)
        seen = s.get("seen", 0)
        rate = s.get("rate", 0.0)
        rss = s.get("rss", 0.0)
        bloom = s.get("bloom", 0.0)
        errors = s.get("errors", 0)
        robots = s.get("robots", 0)
        dupes = s.get("dupes", 0)

        if status == "RUNNING":     sc = 92
        elif status == "PAUSED":    sc = 93
        elif status == "FETCHING":  sc = 96
        elif status.startswith("STOP"): sc = 91
        else:                       sc = 97

        border_col = self.color(90)
        sep = border_col + ("─" if self.unicode else "-") * cols + self.reset()

        lines = []

        title = f" Gateway Crawler v{VERSION} "
        right_plain = f"{status}   {self._fmt_time(elapsed)} "
        gap = max(1, cols - len(title) - len(right_plain) - 2)
        title_line = (
            self.color(96, bold=True) + title + self.reset()
            + " " * gap
            + self.color(sc, bold=True) + status + self.reset()
            + "   " + self.color(37, dim=True) + self._fmt_time(elapsed)
            + " " + self.reset()
        )
        lines.append(self._trunc(title_line, cols))

        lines.append(sep)

        if max_pages and max_pages > 0:
            pct = pages / max_pages
            bar_w = max(10, cols - 30)
            bar = self._bar(pct, bar_w)
            progress = (
                " " + self.color(37) + "Progress " + self.reset()
                + bar + " "
                + self.color(97, bold=True) + f"{pages:,}/{max_pages:,}" + self.reset()
                + " " + self.color(37, dim=True) + f"({pct*100:.1f}%)" + self.reset()
            )
        else:
            progress = (
                " " + self.color(37) + "Pages " + self.reset()
                + self.color(97, bold=True) + f"{pages:,}" + self.reset()
                + "  " + self.color(37, dim=True) + "(no limit)" + self.reset()
            )
        lines.append(self._trunc(progress, cols))

        lines.append("")

        c_lbl = self.color(36)
        c_val = self.color(97, bold=True)
        r1 = (
            " " + c_lbl + "Pages " + self.reset() + c_val + f"{pages:,}" + self.reset()
            + "   " + c_lbl + "Queue " + self.reset() + c_val + f"{queue:,}" + self.reset()
            + "   " + c_lbl + "Seen " + self.reset() + c_val + f"{seen:,}" + self.reset()
            + "   " + c_lbl + "Rate " + self.reset() + c_val + f"{rate:.1f}/min" + self.reset()
        )
        lines.append(self._trunc(r1, cols))

        rss_c = 92 if rss < 80 else (93 if rss < 100 else 91)
        r2 = (
            " " + c_lbl + "RSS " + self.reset()
            + self.color(rss_c) + f"{rss:.1f}MB" + self.reset()
            + "   " + c_lbl + "Bloom " + self.reset() + c_val + f"{bloom*100:.2f}%" + self.reset()
            + "   " + c_lbl + "Err " + self.reset() + c_val + f"{errors:,}" + self.reset()
            + "   " + c_lbl + "Robots " + self.reset() + c_val + f"{robots:,}" + self.reset()
            + "   " + c_lbl + "Dupes " + self.reset() + c_val + f"{dupes:,}" + self.reset()
        )
        lines.append(self._trunc(r2, cols))

        lines.append(sep)

        if self._current:
            cur = (
                " " + self.color(36) + "Current " + self.reset()
                + self.color(37, dim=True) + self._trunc(self._current, cols - 10) + self.reset()
            )
            lines.append(self._trunc(cur, cols))
        else:
            lines.append("")

        reserved = 11
        panel_rows = max(3, rows - reserved)
        panel_rows = min(panel_rows, rows - reserved)

        if self._info_mode:
            lines.extend(self._build_info_panel(cols, panel_rows, s))
        else:
            lines.extend(self._build_activity_panel(cols, panel_rows))

        lines.append(sep)

        hint = (
            " " + self.color(36) + "[q]" + self.reset() + "uit  "
            + self.color(36) + "[p]" + self.reset() + "ause  "
            + self.color(36) + "[i]" + self.reset() + "nfo  "
            + self.color(36) + "[c]" + self.reset() + "lear"
        )
        if self._info_mode:
            hint += "   " + self.color(93) + "(info mode)" + self.reset()
        lines.append(self._trunc(hint, cols))

        return lines

    def _build_activity_panel(self, cols, rows):
        c_hdr = self.color(96, bold=True)
        out = [c_hdr + " Recent Activity" + self.reset()]
        avail = max(0, rows - 1)
        entries = list(self.log_lines)[-avail:]
        for level, msg, _ts in entries:
            icon = self._level_icon(level)
            prefix = f" {icon} "
            body = self._trunc(msg, cols - self._vlen(prefix) - 1)
            out.append(self._trunc(prefix + body, cols))
        return out

    def _build_info_panel(self, cols, rows, s):
        c_hdr = self.color(96, bold=True)
        c_k = self.color(36)
        c_v = self.color(97, bold=True)
        out = [c_hdr + " Detailed Stats" + self.reset()]
        half = max(20, (cols - 2) // 2)

        def fmt_kv(pairs):
            lines = []
            for k, v in pairs:
                line = f"  {c_k}{k:<14}{self.reset()}{c_v}{str(v):<12}{self.reset()}"
                lines.append(self._trunc(line, half))
            return lines

        status_pairs = [
            (f"HTTP {code}", count)
            for code, count in sorted(s.get("status_breakdown", {}).items())
        ]
        type_pairs = [(k or "(none)", v) for k, v in
                      sorted(s.get("content_types", {}).items(),
                             key=lambda kv: -kv[1])[:6]]
        lang_pairs = [(k, v) for k, v in
                      sorted(s.get("languages", {}).items(),
                             key=lambda kv: -kv[1])[:6]]
        dom_pairs = [(k, v) for k, v in
                     list(s.get("top_domains", {}).items())[:6]]

        left = fmt_kv(status_pairs) or ["  (no responses yet)"]
        right = fmt_kv(type_pairs) or ["  (none)"]
        for i in range(min(len(left), len(right))):
            out.append(self._trunc(left[i] + "  " + right[i], cols))
        if len(left) > len(right):
            for i in range(len(right), len(left)):
                out.append(self._trunc(left[i], cols))
        elif len(right) > len(left):
            for i in range(len(left), len(right)):
                out.append(self._trunc(" " * half + right[i], cols))

        out.append("")
        out.append(self.color(96, bold=True) + " Languages" + self.reset())
        if lang_pairs:
            line = "  " + "  ".join(f"{c_k}{k}{self.reset()}={c_v}{v}{self.reset()}"
                                    for k, v in lang_pairs)
        else:
            line = "  (none)"
        out.append(self._trunc(line, cols))

        out.append("")
        out.append(self.color(96, bold=True) + " Top Domains" + self.reset())
        if dom_pairs:
            for k, v in dom_pairs:
                out.append(self._trunc(
                    f"  {c_k}{k:<40}{self.reset()}  {c_v}{v}{self.reset()}", cols))
        else:
            out.append("  (none)")

        return out[:rows]

    @staticmethod
    def _fmt_time(seconds):
        s = int(max(0, seconds))
        h, rem = divmod(s, 3600)
        m, sec = divmod(rem, 60)
        if h:
            return f"{h:02d}:{m:02d}:{sec:02d}"
        return f"{m:02d}:{sec:02d}"


# ---------------------------------------------------------------------------
# HTML parsers
# ---------------------------------------------------------------------------
class MetadataParser(HTMLParser):
    _MAX_TITLE_CHARS = 500  # FIX (#13): limit is by characters, not chunks.

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.title = ""
        self.description = ""
        self.og_description = ""
        self.twitter_description = ""
        self.canonical = ""
        self.author = ""
        self.pub_date = ""
        self.mod_date = ""
        self.lang = ""
        self.headings = []
        self.links = []
        self.jsonld = []
        self.robots_meta = ""
        self._in_title = False
        self._title_parts = []
        self._title_len = 0
        self._in_jsonld = False
        self._jsonld_parts = []
        self._heading_level = 0
        self._heading_parts = []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "title":
            self._in_title = True
        elif tag == "meta":
            n = (a.get("name") or "").lower()
            p = (a.get("property") or "").lower()
            c = (a.get("content") or "").strip()
            if n == "robots" and c:
                self.robots_meta += " " + c.lower()
            elif n == "description" and c and not self.description:
                self.description = c
            elif n == "author" and c and not self.author:
                self.author = c
            elif p == "og:description" and c and not self.og_description:
                self.og_description = c
            elif n == "twitter:description" and c and not self.twitter_description:
                self.twitter_description = c
            elif p in ("article:published_time", "og:published_time") and c and not self.pub_date:
                self.pub_date = c
            elif p in ("article:modified_time", "og:updated_time") and c and not self.mod_date:
                self.mod_date = c
        elif tag == "link":
            rel = (a.get("rel") or "").lower()
            if "canonical" in rel:
                self.canonical = (a.get("href") or "").strip()
        elif tag == "html":
            if not self.lang:
                self.lang = (a.get("lang") or "").strip()
        elif tag in ("h1", "h2", "h3"):
            self._heading_level = int(tag[1])
            self._heading_parts = []
        elif tag == "a":
            href = (a.get("href") or "").strip()
            if href:
                self.links.append(href)
        elif tag == "script":
            if (a.get("type") or "").lower() == "application/ld+json":
                self._in_jsonld = True
                self._jsonld_parts = []

    def handle_endtag(self, tag):
        if tag == "title":
            self._in_title = False
            full = "".join(self._title_parts).strip()
            self.title = full[: self._MAX_TITLE_CHARS]
        elif tag in ("h1", "h2", "h3") and self._heading_level:
            text = "".join(self._heading_parts).strip()
            if text:
                self.headings.append((self._heading_level, text[:300]))
            self._heading_level = 0
            self._heading_parts = []
        elif tag == "script" and self._in_jsonld:
            self._in_jsonld = False
            raw = "".join(self._jsonld_parts).strip()
            if raw:
                try:
                    self.jsonld.append(json.loads(raw))
                except Exception:
                    pass
            self._jsonld_parts = []

    def handle_data(self, data):
        if self._in_title:
            if self._title_len < self._MAX_TITLE_CHARS:
                remaining = self._MAX_TITLE_CHARS - self._title_len
                chunk = data[:remaining]
                self._title_parts.append(chunk)
                self._title_len += len(chunk)
        elif self._in_jsonld:
            self._jsonld_parts.append(data)
        elif self._heading_level and len(self._heading_parts) < 16:
            self._heading_parts.append(data)


class VisibleTextExtractor(HTMLParser):
    _SKIP = {"script", "style", "noscript", "template", "nav",
             "header", "footer", "aside", "form"}
    _PARA = {"p", "article", "main", "section", "div", "li", "blockquote"}

    def __init__(self, max_bytes=200_000):
        super().__init__(convert_charrefs=True)
        self.text_parts = []
        self.paragraphs = []
        self._buf = []
        self._skip_depth = 0
        self._para_depth = 0
        self._bytes = 0
        self._max = max_bytes

    def handle_starttag(self, tag, attrs):
        if tag in self._SKIP:
            self._skip_depth += 1
        elif tag in self._PARA:
            self._para_depth += 1
            if self._buf:
                self._flush_para()

    def handle_endtag(self, tag):
        if tag in self._SKIP and self._skip_depth > 0:
            self._skip_depth -= 1
        elif tag in self._PARA and self._para_depth > 0:
            self._para_depth -= 1
            if self._buf:
                self._flush_para()

    def handle_data(self, data):
        if self._skip_depth > 0 or self._bytes >= self._max:
            return
        s = data.strip()
        if not s:
            return
        self._bytes += len(s)
        self.text_parts.append(s)
        self._buf.append(s)

    def _flush_para(self):
        text = re.sub(r"\s+", " ", " ".join(self._buf)).strip()
        self._buf = []
        if len(text) >= 40:
            self.paragraphs.append(text)

    def finish(self):
        if self._buf:
            self._flush_para()
        return {
            "text": re.sub(r"\s+", " ", " ".join(self.text_parts)).strip(),
            "paragraphs": self.paragraphs,
        }


# ---------------------------------------------------------------------------
# Content analysis
# ---------------------------------------------------------------------------
def dedupe_paragraphs(paras, max_keep=50):
    seen = set()
    out = []
    for p in paras:
        h = hashlib.md5(p.encode("utf-8")).hexdigest()
        if h in seen:
            continue
        seen.add(h)
        out.append(p)
        if len(out) >= max_keep:
            break
    return out


def word_count(text):
    return len(re.findall(r"\w+", text))


def reading_time_minutes(wc, wpm=200):
    return max(1, round(wc / wpm)) if wc > 0 else 0


def pick_description(meta, paragraphs, max_len=300):
    for src in (meta.get("description"), meta.get("og_description"),
                meta.get("twitter_description")):
        if src:
            s = re.sub(r"\s+", " ", src).strip()
            if s:
                return s[:max_len]
    for p in paragraphs:
        if len(p) >= 80:
            return p[:max_len]
    return (paragraphs[0][:max_len] if paragraphs else "")


def detect_language_from_text(text, sample=500):
    words = re.findall(r"\b[a-z]{2,}\b", text[:sample].lower())
    if not words:
        return ""
    en_hits = sum(1 for w in words if w in STOPWORDS)
    return "en" if en_hits / len(words) > 0.05 else ""


def jsonld_extract(objs, keys):
    found = {}
    stack = list(objs)
    while stack:
        o = stack.pop()
        if isinstance(o, dict):
            for k, v in o.items():
                if k in keys and isinstance(v, (str, int, float)) and k not in found:
                    found[k] = str(v)
            for v in o.values():
                if isinstance(v, (dict, list)):
                    stack.append(v)
        elif isinstance(o, list):
            stack.extend(o)
    return found


def detect_encoding(body, header_charset):
    if header_charset:
        try:
            codecs.lookup(header_charset)
            return header_charset
        except LookupError:
            pass
    if body.startswith(b"\xef\xbb\xbf"): return "utf-8-sig"
    if body.startswith(b"\xff\xfe"):     return "utf-16-le"
    if body.startswith(b"\xfe\xff"):     return "utf-16-be"
    m = re.search(rb'<meta[^>]+charset\s*=\s*["\']?([\w\-]+)', body[:4096], re.I)
    if m:
        try:
            enc = m.group(1).decode("ascii", "ignore").lower()
            codecs.lookup(enc)
            return enc
        except (LookupError, UnicodeDecodeError):
            pass
    m = re.search(rb'<meta[^>]+content\s*=\s*["\'][^"\']*charset\s*=\s*([\w\-]+)',
                  body[:4096], re.I)
    if m:
        try:
            enc = m.group(1).decode("ascii", "ignore").lower()
            codecs.lookup(enc)
            return enc
        except (LookupError, UnicodeDecodeError):
            pass
    return "utf-8"


# ---------------------------------------------------------------------------
# Sitemap / feed parsing
# ---------------------------------------------------------------------------
def parse_sitemap_bytes(data, depth=0, max_depth=2):
    if depth > max_depth or not data:
        return
    try:
        root = ET.fromstring(data)
    except Exception:
        return
    tag = root.tag.rsplit("}", 1)[-1]
    if tag == "sitemapindex":
        for loc in root.iter():
            if loc.tag.rsplit("}", 1)[-1] == "loc" and loc.text:
                yield ("index", loc.text.strip())
    elif tag == "urlset":
        for url in root.iter():
            if url.tag.rsplit("}", 1)[-1] == "loc" and url.text:
                yield ("url", url.text.strip())


def parse_sitemap_txt(text):
    for line in text.splitlines():
        line = line.strip()
        if line and line.startswith(("http://", "https://")):
            yield ("url", line)


def parse_feed(data):
    try:
        root = ET.fromstring(data)
    except Exception:
        return []
    items = []
    tag = root.tag.rsplit("}", 1)[-1]
    if tag == "rss":
        for item in root.iter():
            if item.tag.rsplit("}", 1)[-1] != "item":
                continue
            title, link = "", ""
            for child in item:
                ct = child.tag.rsplit("}", 1)[-1]
                if ct == "title" and child.text:
                    title = child.text.strip()
                elif ct == "link" and child.text:
                    link = child.text.strip()
            if link:
                items.append((title, link))
    elif tag == "feed":
        for entry in root.iter():
            if entry.tag.rsplit("}", 1)[-1] != "entry":
                continue
            title, link = "", ""
            for child in entry:
                ct = child.tag.rsplit("}", 1)[-1]
                if ct == "title" and child.text:
                    title = child.text.strip()
                elif ct == "link" and child.get("href"):
                    rel = child.get("rel", "alternate")
                    if rel in ("alternate", "") and not link:
                        link = child.get("href")
            if link:
                items.append((title, link))
    return items


# ---------------------------------------------------------------------------
# Robots
# ---------------------------------------------------------------------------
def _fetch_robots_txt(url, ua, timeout, max_bytes=DEFAULT_ROBOTS_BYTES):
    req = urllib.request.Request(
        url, headers={"User-Agent": ua, "Accept-Encoding": "gzip, deflate"})
    try:
        # FIX (#2): use SSRF-safe opener for robots.txt fetches too.
        resp = safe_urlopen(req, timeout=timeout)
    except urllib.error.HTTPError as e:
        if e.code in (401, 403):
            return None, "disallow_all"
        if 400 <= e.code < 500:
            return None, "allow_all"
        return None, "error"
    except Exception:
        return None, "error"
    try:
        data = resp.read(max_bytes)
        enc = (resp.headers.get("Content-Encoding") or "").lower()
        if "gzip" in enc:
            try: data = gzip.decompress(data)
            except Exception: pass
        elif "deflate" in enc:
            try: data = zlib.decompress(data)
            except Exception: pass
        cs = resp.headers.get_content_charset() or "utf-8"
        try: text = data.decode(cs, errors="replace")
        except LookupError: text = data.decode("utf-8", errors="replace")
        return text, "ok"
    except Exception:
        return None, "error"
    finally:
        try: resp.close()
        except Exception: pass


class RobotsCache:
    def __init__(self, ua, timeout=DEFAULT_TIMEOUT, maxsize=DEFAULT_ROBOTS_CAP):
        self.ua = ua
        self.timeout = timeout
        self._cache = BoundedDict(maxsize)

    def _url_for(self, url):
        p = urllib.parse.urlparse(url)
        return f"{p.scheme}://{p.netloc}/robots.txt"

    def _get(self, url):
        key = self._url_for(url)
        if key in self._cache:
            self._cache.move_to_end(key)
            return self._cache[key]
        text, status = _fetch_robots_txt(key, self.ua, self.timeout)
        entry = {"status": status, "rp": None, "delay": None}
        if status == "ok" and text is not None:
            rp = urllib.robotparser.RobotFileParser()
            rp.set_url(key)
            try:
                rp.parse(text.splitlines())
                entry["rp"] = rp
                try:
                    d = rp.crawl_delay(self.ua)
                    if d: entry["delay"] = float(d)
                except Exception:
                    pass
            except Exception:
                entry["status"] = "error"
        self._cache[key] = entry
        return entry

    def can_fetch(self, url):
        e = self._get(url)
        if e["status"] == "disallow_all": return False
        if e["status"] in ("allow_all", "error"): return True
        rp = e["rp"]
        if rp is None: return True
        try: return rp.can_fetch(self.ua, url)
        except Exception: return True

    def crawl_delay(self, url):
        return self._get(url).get("delay")


# ---------------------------------------------------------------------------
# Fetcher
# ---------------------------------------------------------------------------
class FetchResult:
    __slots__ = ("final_url", "status", "content_type", "language",
                 "html_bytes", "meta", "content", "error", "from_cache",
                 "x_robots_tag", "_charset", "_body")
    def __init__(self):
        self.final_url = ""
        self.status = 0
        self.content_type = ""
        self.language = ""
        self.html_bytes = 0
        self.meta = None
        self.content = None
        self.error = ""
        self.from_cache = False
        self.x_robots_tag = ""
        self._charset = None
        self._body = None


class _HTTPError(Exception):
    def __init__(self, code, retry_after=None):
        self.code = code
        self.retry_after = retry_after
        super().__init__(f"HTTP {code}")


class _URLError(Exception):
    pass


class Fetcher:
    def __init__(self, ua=DEFAULT_USER_AGENT, timeout=DEFAULT_TIMEOUT,
                 max_retries=DEFAULT_MAX_RETRIES,
                 page_max_bytes=DEFAULT_PAGE_BYTES,
                 chunk=DEFAULT_CHUNK, log=None):
        self.ua = ua
        self.timeout = timeout
        self.max_retries = max_retries
        self.page_max_bytes = page_max_bytes
        self.chunk = chunk
        self.log = log or Logger()
        self.headers = {
            "User-Agent": ua,
            "Accept": "text/html,application/xhtml+xml;q=0.9,*/*;q=0.5",
            "Accept-Language": "en-US,en;q=0.5",
            "Accept-Encoding": "gzip, deflate",
            "Connection": "close",
        }
        self._conditional = BoundedDict(10_000)

    def _do_request(self, url):
        self.log.debug(f"request preparation: {url}")
        safe, reason = is_safe_to_fetch(url)
        if not safe:
            raise RuntimeError(f"ssrf blocked: {reason}")

        etag, lm = self._conditional.get(url, (None, None))
        headers = dict(self.headers)
        if etag: headers["If-None-Match"] = etag
        if lm:   headers["If-Modified-Since"] = lm

        req = urllib.request.Request(url, headers=headers)
        try:
            # FIX (#2): use SSRF-safe opener.
            resp = safe_urlopen(req, timeout=self.timeout)
        except urllib.error.HTTPError as e:
            if e.code == 304:
                r = FetchResult(); r.final_url = url; r.status = 304
                r.from_cache = True
                return r, None
            retry_after = None
            try:
                ra = e.headers.get("Retry-After") if e.headers else None
                if ra:
                    try: retry_after = float(ra)
                    except ValueError: retry_after = None
            except Exception:
                pass
            raise _HTTPError(e.code, retry_after=retry_after) from e
        except urllib.error.URLError as e:
            raise _URLError(str(e.reason)) from e
        except socket.timeout:
            raise _URLError("timeout") from None

        enc = (resp.headers.get("Content-Encoding") or "").lower()
        decomp = None
        if "gzip" in enc:     decomp = zlib.decompressobj(16 + zlib.MAX_WBITS)
        elif "deflate" in enc: decomp = zlib.decompressobj()

        chunks = []
        total = 0
        try:
            while total < self.page_max_bytes:
                try: raw = resp.read(self.chunk)
                except socket.timeout: break
                except Exception: break
                if not raw: break
                if decomp is not None:
                    try:
                        # FIX (#7): don't use max_length (which would leave
                        # data in unconsumed_tail and drop the tail of the
                        # stream). Read all, then truncate below.
                        data = decomp.decompress(raw)
                    except zlib.error:
                        break
                else:
                    data = raw
                if not data:
                    continue
                if total + len(data) > self.page_max_bytes:
                    data = data[: self.page_max_bytes - total]
                total += len(data)
                chunks.append(data)
                if total >= self.page_max_bytes:
                    break
            # FIX (#7): flush the decompressor to recover the tail.
            if decomp is not None and total < self.page_max_bytes:
                try:
                    tail = decomp.flush()
                except Exception:
                    tail = b""
                if tail:
                    if total + len(tail) > self.page_max_bytes:
                        tail = tail[: self.page_max_bytes - total]
                    total += len(tail)
                    chunks.append(tail)
        finally:
            try: resp.close()
            except Exception: pass

        body = b"".join(chunks)

        # FIX (#5): read attributes now (they're cached on HTTPResponse,
        # but this also avoids relying on that behavior).
        r = FetchResult()
        r.final_url = resp.geturl()
        r.status = resp.status
        r.content_type = (resp.headers.get("Content-Type") or "").split(";")[0].strip().lower()
        r.language = (resp.headers.get("Content-Language") or "").strip()
        r.x_robots_tag = (resp.headers.get("X-Robots-Tag") or "").strip()
        r.html_bytes = total
        self.log.debug(f"response received: status={r.status}, bytes={total}, type={r.content_type or '(unknown)'}, url={r.final_url}")

        etag = resp.headers.get("ETag")
        lm = resp.headers.get("Last-Modified")
        if etag or lm:
            self._conditional[url] = (etag, lm)

        r._charset = (resp.headers.get_content_charset()
                      if hasattr(resp.headers, "get_content_charset") else None)
        r._body = body
        return r, body

    def fetch(self, url):
        attempt = 0
        while True:
            attempt += 1
            try:
                result, body = self._do_request(url)
                if result.from_cache:
                    return result, None
                if body is None:
                    result.error = "no body"
                    return result, None
                self.log.debug(f"parsing response: {len(body)} bytes from {result.final_url}")
                meta, content = self._parse(
                    body, result.content_type, result.final_url,
                    result._charset)
                self.log.debug(f"parse complete: title={(meta or {}).get('title', '')!r}, words={(content or {}).get('word_count', 0)}")
                result.meta = meta
                result.content = content
                return result, None
            except _HTTPError as e:
                if e.code in (429, 500, 502, 503, 504) and attempt <= self.max_retries:
                    delay = e.retry_after if e.retry_after is not None else (2 ** attempt)
                    delay = min(delay, 60)
                    self.log.debug(f"retry in {delay:.1f}s on HTTP {e.code}: {url}")
                    deadline = time.time() + delay
                    while time.time() < deadline:
                        time.sleep(0.1)
                    continue
                r = FetchResult(); r.final_url = url; r.status = e.code
                r.error = f"HTTP {e.code}"
                return r, None
            except _URLError as e:
                if attempt <= self.max_retries:
                    deadline = time.time() + min(2 ** attempt, 30)
                    while time.time() < deadline:
                        time.sleep(0.1)
                    continue
                r = FetchResult(); r.final_url = url
                r.error = str(e)
                return r, None
            except Exception as e:
                r = FetchResult(); r.final_url = url
                r.error = f"unexpected: {e}"
                return r, None

    def _parse(self, body, content_type, base_url, header_charset=None):
        if "html" not in content_type and content_type not in ("", "text/plain"):
            return None, None
        enc = detect_encoding(body, header_charset)
        try:
            text = body.decode(enc, errors="replace")
        except LookupError:
            text = body.decode("utf-8", errors="replace")
        meta = MetadataParser()
        vis = VisibleTextExtractor()
        try: meta.feed(text)
        except Exception: pass
        try: vis.feed(text)
        except Exception: pass
        content = vis.finish()
        content["paragraphs"] = dedupe_paragraphs(content["paragraphs"])
        content["word_count"] = word_count(content["text"])
        content["reading_minutes"] = reading_time_minutes(content["word_count"])
        return {
            "title": meta.title,
            "description": meta.description,
            "og_description": meta.og_description,
            "twitter_description": meta.twitter_description,
            "canonical": meta.canonical,
            "author": meta.author,
            "pub_date": meta.pub_date,
            "mod_date": meta.mod_date,
            "lang": meta.lang,
            "headings": meta.headings,
            "links": meta.links,
            "jsonld": meta.jsonld,
            "robots_meta": meta.robots_meta,
        }, content


# ---------------------------------------------------------------------------
# NEW: ntfy.sh notifier
#
# Pushes notifications via https://ntfy.sh/<topic> (or a self-hosted server
# by passing a full URL as the topic). No token or account required.
#
# The crawler automatically emits messages for its own events (start, finish,
# periodic progress, errors, interrupts). See GatewayCrawler._notify().
# ---------------------------------------------------------------------------
class NtfyNotifier:
    def __init__(self, topic, log=None, timeout=DEFAULT_NTFY_TIMEOUT):
        topic = (topic or "").strip()
        if not topic:
            raise ValueError("ntfy topic is required")
        if topic.startswith(("http://", "https://")):
            self.url = topic
        else:
            self.url = f"{DEFAULT_NTFY_SERVER.rstrip('/')}/{topic}"
        self.log = log
        self.timeout = timeout
        self.last_error = None

    def send(self, message, title=None, priority=3, tags=None, click=None):
        headers = {
            "Content-Type": "text/plain; charset=utf-8",
            "Priority": str(priority),
        }
        if title: headers["Title"] = title
        if tags:
            if isinstance(tags, (list, tuple)):
                headers["Tags"] = ",".join(str(t) for t in tags)
            else:
                headers["Tags"] = str(tags)
        if click: headers["Click"] = click
        try:
            req = urllib.request.Request(
                self.url,
                data=str(message).encode("utf-8"),
                headers=headers,
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                ok = 200 <= r.status < 300
                if not ok and self.log:
                    self.log.warn(f"ntfy returned HTTP {r.status}")
                return ok
        except Exception as e:
            self.last_error = str(e)
            if self.log:
                self.log.warn(f"ntfy notify failed: {e}")
            return False


# ---------------------------------------------------------------------------
# Writers
# ---------------------------------------------------------------------------
class JsonArrayWriter:
    def __init__(self, path):
        self.path = path
        self.tmp = path + ".part"
        self._fh = open(self.tmp, "w", encoding="utf-8")
        self._fh.write("[\n")
        self._fh.flush()
        self._first = True
        self._count = 0
        self._closed = False

    def write(self, obj):
        line = ("  " if self._first else ",\n  ") + json.dumps(obj, ensure_ascii=False)
        self._fh.write(line)
        self._fh.flush()
        self._first = False
        self._count += 1

    def close(self):
        if self._closed:
            return
        self._closed = True
        try:
            self._fh.write("\n]\n" if self._count else "]\n")
            self._fh.flush()
        except Exception:
            pass
        try:
            os.fsync(self._fh.fileno())
        except Exception:
            pass
        try:
            self._fh.close()
        except Exception:
            pass
        try:
            os.replace(self.tmp, self.path)
        except Exception:
            try:
                shutil.move(self.tmp, self.path)
            except Exception:
                pass


class NullWriter:
    def write(self, obj): pass
    def close(self): pass


class SQLiteWriter:
    def __init__(self, path):
        self.path = path
        self._conn = sqlite3.connect(path)
        self._conn.execute("""
            CREATE TABLE IF NOT EXISTS pages (
                id TEXT PRIMARY KEY, url TEXT NOT NULL, title TEXT,
                description TEXT, domain TEXT, status INTEGER,
                content_type TEXT, language TEXT, author TEXT,
                pub_date TEXT, canonical TEXT, word_count INTEGER,
                content_hash TEXT, crawled_at REAL
            )
        """)
        self._conn.execute("CREATE INDEX IF NOT EXISTS idx_domain ON pages(domain)")
        self._conn.execute("CREATE INDEX IF NOT EXISTS idx_lang ON pages(language)")
        self._conn.commit()
        self._count = 0

    def write(self, obj):
        try:
            self._conn.execute(
                "INSERT OR REPLACE INTO pages VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (obj["id"], obj["u"], obj["t"], obj["d"], obj["s"],
                 obj.get("status", 0), obj.get("content_type", ""),
                 obj.get("lang", ""), obj.get("author", ""),
                 obj.get("pub_date", ""), obj.get("canonical", ""),
                 obj.get("word_count", 0), obj.get("content_hash", ""),
                 time.time()))
            self._conn.commit()
            self._count += 1
        except Exception:
            pass

    def close(self):
        try: self._conn.close()
        except Exception: pass


# ---------------------------------------------------------------------------
# Checkpoint
# ---------------------------------------------------------------------------
class Checkpoint:
    def __init__(self, path):
        self.path = path

    def save(self, crawler):
        try:
            data = {
                "version": VERSION,
                "bloom": crawler.seen.to_dict(),
                "frontier": crawler.frontier.snapshot(),
                "page_count": crawler.page_count,
                "stats": crawler.stats.to_dict(),
                "seeds": crawler.seeds,
                # FIX (#4): persist per-domain counts so resume honours --max-per-domain.
                "domain_pages": dict(crawler._domain_pages),
            }
            tmp = self.path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(data, f)
            os.replace(tmp, self.path)
        except Exception as e:
            if crawler.log:
                crawler.log.warn(f"checkpoint save failed: {e}")

    def load(self):
        if not os.path.exists(self.path):
            return None
        try:
            with open(self.path, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return None


# ---------------------------------------------------------------------------
# Stats
# ---------------------------------------------------------------------------
class Stats:
    def __init__(self):
        self.status = Counter()
        self.domains = Counter()
        self.languages = Counter()
        self.types = Counter()
        self.errors = Counter()
        self.failed_urls = []
        self.total_bytes = 0
        self.start = time.time()

    def record(self, result):
        self.status[result.status] += 1
        if result.content_type:
            self.types[result.content_type] += 1
        self.total_bytes += result.html_bytes
        if result.language:
            self.languages[result.language] += 1

    def record_fail(self, url, reason):
        self.errors[reason[:80]] += 1
        if len(self.failed_urls) < 500:
            self.failed_urls.append({"url": url, "reason": reason[:200]})

    def to_dict(self):
        return {
            "status": dict(self.status),
            "domains": dict(self.domains.most_common(200)),
            "languages": dict(self.languages),
            "types": dict(self.types),
            "errors": dict(self.errors),
            "failed_urls": self.failed_urls,
            "total_bytes": self.total_bytes,
        }

    def restore(self, d):
        """FIX (#4): restore counters after --resume."""
        if not d:
            return
        self.status.update(d.get("status", {}))
        self.domains.update(d.get("domains", {}))
        self.languages.update(d.get("languages", {}))
        self.types.update(d.get("types", {}))
        self.errors.update(d.get("errors", {}))
        self.total_bytes = d.get("total_bytes", 0)
        self.failed_urls = list(d.get("failed_urls", []))

    def summary(self, page_count):
        elapsed = max(time.time() - self.start, 0.001)
        avg_size = self.total_bytes / max(page_count, 1)
        return {
            "pages": page_count,
            "elapsed_seconds": round(elapsed, 1),
            "pages_per_minute": round(page_count / elapsed * 60, 1),
            "avg_page_bytes": int(avg_size),
            "status_breakdown": dict(self.status),
            "content_types": dict(self.types),
            "languages": dict(self.languages),
            "top_domains": dict(self.domains.most_common(10)),
            "errors": dict(self.errors),
            "failed_url_count": len(self.failed_urls),
        }


# ---------------------------------------------------------------------------
# Crawler
# ---------------------------------------------------------------------------
class GatewayCrawler:
    def __init__(
        self,
        seeds,
        max_pages=None,
        delay=DEFAULT_DELAY,
        output_path=DEFAULT_OUTPUT,
        user_agent=DEFAULT_USER_AGENT,
        timeout=DEFAULT_TIMEOUT,
        same_host_only=False,
        verbose=True,
        quiet=False,
        max_queue_size=DEFAULT_MAX_QUEUE,
        bloom_capacity=DEFAULT_BLOOM_CAP,
        bloom_fp=DEFAULT_BLOOM_FP,
        max_page_bytes=DEFAULT_PAGE_BYTES,
        chunk_size=DEFAULT_CHUNK,
        max_memory_mb=None,
        ignore_robots=False,
        verbose_robots=False,
        max_depth=DEFAULT_MAX_DEPTH,
        max_per_domain=DEFAULT_MAX_PER_DOM,
        max_links_per_page=DEFAULT_MAX_LINKS,
        exclude_domains=None,
        exclude_patterns=None,
        use_sitemaps=False,
        use_feeds=False,
        storage="json",
        checkpoint_path=DEFAULT_CHECKPOINT,
        resume=False,
        dry_run=False,
        max_retries=DEFAULT_MAX_RETRIES,
        log_fmt="text",
        language_filter=None,
        respect_noindex=True,
        tui=None,
        debug_log=None,
        # NEW: ntfy integration
        ntfy_notifier=None,
        ntfy_events=None,
        ntfy_progress_every=0,
    ):
        cleaned = [n for n in (normalize_url(s) for s in seeds) if n]
        if not cleaned:
            raise ValueError("No valid seed URLs provided.")

        self.seeds = cleaned
        self.max_pages = max_pages
        self.delay = delay
        self.output_path = output_path
        self.user_agent = user_agent
        self.timeout = timeout
        self.same_host_only = same_host_only
        self.max_page_bytes = max_page_bytes
        self.chunk_size = chunk_size
        self.max_memory_mb = max_memory_mb
        self.ignore_robots = ignore_robots
        self.verbose_robots = verbose_robots
        self.max_depth = max_depth
        self.max_per_domain = max_per_domain
        self.max_links_per_page = max_links_per_page
        self.exclude_domains = [re.compile(p) for p in (exclude_domains or [])]
        self.exclude_patterns = [re.compile(p) for p in (exclude_patterns or [])]
        self.use_sitemaps = use_sitemaps
        self.use_feeds = use_feeds
        self.storage = storage
        self.checkpoint_path = checkpoint_path
        self.resume = resume
        self.dry_run = dry_run
        self.language_filter = set(l.lower() for l in (language_filter or []))
        self.respect_noindex = respect_noindex
        self.tui = tui

        # NEW: ntfy state
        self.notifier = ntfy_notifier
        self.ntfy_events = set(e.lower() for e in (ntfy_events or []))
        self.ntfy_progress_every = int(ntfy_progress_every or 0)

        self.log = Logger(verbose=verbose, quiet=quiet, fmt=log_fmt, tui=tui,
                          log_file=debug_log)
        if self.notifier:
            self.notifier.log = self.log
        if debug_log:
            self.log.info(f"Detailed log file: {debug_log}")
        self.fetcher = Fetcher(
            ua=user_agent, timeout=timeout, max_retries=max_retries,
            page_max_bytes=max_page_bytes, chunk=chunk_size, log=self.log)
        self.robots = RobotsCache(user_agent, timeout=timeout)
        self._last_request = BoundedDict(DEFAULT_RATELIM_CAP)
        self._domain_pages = Counter()
        self._robots_explained = BoundedDict(500)

        self.seen = BloomFilter(capacity=bloom_capacity, error_rate=bloom_fp)
        self.seen_content = BloomFilter(capacity=bloom_capacity, error_rate=bloom_fp)
        self.frontier = Frontier(max_queue_size)

        for s in self.seeds:
            if s not in self.seen:
                self.seen.add(s)
                self.frontier.push(s, 0, None)

        self.page_count = 0
        self.robots_blocks = 0
        self.network_errors = 0
        self.duplicate_content = 0
        self.filtered = 0
        self.noindex_skipped = 0
        self._stop = False
        self._stop_reason = "unknown"
        self.paused = False
        self.stats = Stats()

        if dry_run:
            self._writer = NullWriter()
        elif storage == "sqlite":
            self._writer = SQLiteWriter(output_path)
        else:
            self._writer = JsonArrayWriter(output_path)

        # FIX (#6): signal.signal() raises ValueError outside the main thread.
        # Wrap so GatewayCrawler can be used as a library from a worker thread.
        try:
            signal.signal(signal.SIGINT, self._on_signal)
            try:
                signal.signal(signal.SIGTERM, self._on_signal)
            except Exception:
                pass
        except ValueError:
            self.log.debug("signal handlers not installed (non-main thread)")

        if self.resume:
            self._restore_from_checkpoint()

    # -- ntfy --------------------------------------------------------------
    def _notify(self, event, message, title=None, priority=3, tags=None,
                click=None):
        """Fire an ntfy notification if the notifier exists and the event
        is in the configured set. Never raises."""
        if not self.notifier:
            return
        if event not in self.ntfy_events:
            return
        try:
            self.notifier.send(message, title=title, priority=priority,
                               tags=tags, click=click)
        except Exception as e:
            try:
                self.log.warn(f"ntfy dispatch failed: {e}")
            except Exception:
                pass

    # -- signals -----------------------------------------------------------
    def _on_signal(self, signum, frame):
        if self._stop:
            self.log.error("second signal; forcing exit")
            try: self._cleanup()
            except Exception: pass
            if self.tui:
                self.tui.exit()
            sys.exit(1)
        self.log.warn(f"signal {signum} received; finishing current request")
        self._stop = True

    def _restore_from_checkpoint(self):
        cp = Checkpoint(self.checkpoint_path).load()
        if not cp:
            self.log.info("no checkpoint found; starting fresh")
            return
        try:
            self.seen = BloomFilter.from_dict(cp["bloom"])
            self.frontier = Frontier(self.frontier.maxsize)
            for url, depth, base in cp.get("frontier", []):
                self.frontier.push(url, depth, base)
            self.page_count = cp.get("page_count", 0)
            # FIX (#4): restore domain counters and stats so --max-per-domain
            # and the final report behave correctly across resumes.
            self._domain_pages.update(cp.get("domain_pages", {}))
            self.stats.restore(cp.get("stats", {}))
            self.log.info(
                f"resumed from checkpoint: {self.page_count} pages, "
                f"{len(self.frontier)} URLs in frontier")
        except Exception as e:
            self.log.warn(f"checkpoint restore failed: {e}")

    # -- helpers -----------------------------------------------------------
    def _polite_wait(self, url):
        host = get_domain(url)
        delay = self.delay
        try:
            cd = self.robots.crawl_delay(url)
            if cd and cd > delay:
                delay = cd
        except Exception:
            pass
        now = time.time()
        elapsed = now - self._last_request.get(host, 0)
        remaining = delay - elapsed
        if remaining > 0:
            deadline = time.time() + remaining
            while time.time() < deadline:
                if self._stop:
                    return
                # FIX (#11): keep the TUI responsive during the polite wait.
                if self.tui and self.tui.enabled:
                    if self._tui_pump():
                        return
                time.sleep(0.1)
        self._last_request[host] = time.time()

    def _url_excluded(self, url):
        d = get_domain(url)
        for rx in self.exclude_domains:
            if rx.search(d): return True
        for rx in self.exclude_patterns:
            if rx.search(url): return True
        return False

    def _domain_capped(self, url):
        return self._domain_pages[get_domain(url)] >= self.max_per_domain

    def _build_result(self, url, result):
        meta = result.meta or {}
        content = result.content or {}
        final_url = result.final_url or url
        if meta.get("canonical"):
            c = normalize_url(meta["canonical"], final_url)
            if c: final_url = c

        paragraphs = content.get("paragraphs", [])
        text = content.get("text", "")
        wc = content.get("word_count", 0)
        rt = content.get("reading_minutes", 0)
        description = pick_description(meta, paragraphs)

        title = re.sub(r"\s+", " ", (meta.get("title") or "")).strip()
        if not title:
            title = final_url
        if len(title) > 200:
            title = title[:197] + "..."

        lang = (meta.get("lang") or result.language or "").lower()
        if not lang:
            lang = detect_language_from_text(text)

        jsonld = meta.get("jsonld") or []
        ld = jsonld_extract(jsonld, {
            "@type", "author", "headline", "datePublished",
            "dateModified", "publisher", "description"})

        return {
            "id": stable_id(final_url),
            "t": title,
            "u": final_url,
            "d": description,
            "s": get_domain(final_url),
            "status": result.status,
            "content_type": result.content_type,
            "lang": lang,
            "author": (meta.get("author") or ld.get("author") or ""),
            "pub_date": (meta.get("pub_date") or ld.get("datePublished") or ""),
            "mod_date": (meta.get("mod_date") or ld.get("dateModified") or ""),
            "canonical": final_url,
            "word_count": wc,
            "reading_minutes": rt,
            "content_hash": content_hash(text) if text else "",
        }, (meta.get("links") or []), meta.get("robots_meta", "")

    def _write(self, obj):
        if self.dry_run:
            return
        self._writer.write(obj)
        self.page_count += 1

    def _enqueue(self, url, depth, base_url):
        if url in self.seen:
            return
        if self._url_excluded(url):
            self.filtered += 1
            return
        if self.same_host_only and base_url and not is_same_host(base_url, url):
            return
        if depth > self.max_depth:
            return
        if self._domain_capped(url):
            return
        if re.search(
            r"\.(?:jpg|jpeg|png|gif|webp|pdf|zip|tar|gz|tgz|mp4|mp3|avi|mov|"
            r"exe|dmg|iso|css|js|woff2?|ttf|eot|svg)(?:\?|$)",
            url, re.I):
            return
        if self.frontier.push(url, depth, base_url):
            self.seen.add(url)
            self.log.debug(f"queued link: depth={depth}, url={url}")

    # -- sitemap / feeds ---------------------------------------------------
    def _discover_sitemaps(self):
        if not self.use_sitemaps:
            return
        visited = set()
        for seed in self.seeds:
            domain = get_domain(seed)
            if domain in visited: continue
            visited.add(domain)
            p = urllib.parse.urlparse(seed)
            base = f"{p.scheme}://{p.netloc}"
            for path in ("/sitemap.xml", "/sitemap_index.xml", "/sitemap.txt"):
                self._ingest_sitemap_url(base + path, depth=0)

    def _ingest_sitemap_url(self, url, depth):
        # FIX (#10): honour robots.txt for the sitemap file itself.
        if not self.ignore_robots and not self.robots.can_fetch(url):
            self.robots_blocks += 1
            self.log.debug(f"sitemap blocked by robots.txt: {url}")
            return
        self.log.info(f"sitemap: {url}")
        try:
            safe, _ = is_safe_to_fetch(url)
            if not safe: return
            req = urllib.request.Request(url, headers={"User-Agent": self.user_agent})
            data = safe_urlopen(req, timeout=self.timeout).read(2_000_000)
        except Exception as e:
            self.log.debug(f"sitemap fetch failed: {e}")
            return
        if b"<sitemapindex" in data or b"<urlset" in data:
            for kind, u in parse_sitemap_bytes(data, depth=depth):
                if kind == "index":
                    if depth < 2: self._ingest_sitemap_url(u, depth + 1)
                else:
                    norm = normalize_url(u)
                    if norm and norm not in self.seen:
                        # FIX (#10): same-host guard for sitemap URLs.
                        if self.same_host_only and not any(
                                is_same_host(s, norm) for s in self.seeds):
                            continue
                        self.seen.add(norm)
                        self.frontier.push(norm, 0, None)
        elif data[:50].lstrip().startswith(b"http"):
            for kind, u in parse_sitemap_txt(data.decode("utf-8", "replace")):
                norm = normalize_url(u)
                if norm and norm not in self.seen:
                    if self.same_host_only and not any(
                            is_same_host(s, norm) for s in self.seeds):
                        continue
                    self.seen.add(norm)
                    self.frontier.push(norm, 0, None)

    def _discover_feeds(self):
        if not self.use_feeds:
            return
        seen = set()
        for seed in self.seeds:
            p = urllib.parse.urlparse(seed)
            base = f"{p.scheme}://{p.netloc}"
            if base in seen: continue
            seen.add(base)
            for path in ("/feed", "/rss", "/rss.xml", "/atom.xml", "/feed.xml"):
                url = base + path
                # FIX (#10): honour robots.txt for the feed file itself.
                if not self.ignore_robots and not self.robots.can_fetch(url):
                    self.robots_blocks += 1
                    continue
                try:
                    safe, _ = is_safe_to_fetch(url)
                    if not safe: continue
                    req = urllib.request.Request(url, headers={"User-Agent": self.user_agent})
                    data = safe_urlopen(req, timeout=self.timeout).read(2_000_000)
                except Exception:
                    continue
                items = parse_feed(data)
                if items:
                    self.log.info(f"feed: {url} ({len(items)} items)")
                    for _, link in items:
                        norm = normalize_url(link)
                        if norm and norm not in self.seen:
                            if self.same_host_only and not any(
                                    is_same_host(s, norm) for s in self.seeds):
                                continue
                            self.seen.add(norm)
                            self.frontier.push(norm, 0, None)

    def _disk_ok(self):
        try:
            usage = shutil.disk_usage(
                os.path.dirname(os.path.abspath(self.output_path)) or ".")
            return usage.free > 50 * 1024 * 1024
        except Exception:
            return True

    def _validate_output_file(self):
        try:
            if not os.path.exists(self.output_path):
                return False, "file does not exist"
            size = os.path.getsize(self.output_path)
            if size < 2:
                return False, f"file too small ({size} bytes)"
            with open(self.output_path, "rb") as f:
                first = f.read(1)
                if first != b"[":
                    return False, f"first byte is {first!r}, expected b'['"
                f.seek(max(0, size - 128))
                tail = f.read()
            stripped = tail.rstrip()
            if not stripped.endswith(b"]"):
                return False, f"last non-whitespace byte is {stripped[-1:]!r}, expected b']'"
            return True, "ok"
        except Exception as e:
            return False, f"validation error: {e}"

    # -- TUI glue ----------------------------------------------------------
    def _get_state(self, status=None):
        elapsed = time.time() - self.stats.start
        rate = (self.page_count / elapsed * 60) if elapsed > 0 else 0
        if status is None:
            if self._stop:
                status = "STOPPED"
            elif self.paused:
                status = "PAUSED"
            else:
                status = "RUNNING"
        return {
            "status": status,
            "pages": self.page_count,
            "max_pages": self.max_pages,
            "queue": len(self.frontier),
            "seen": self.seen.added,
            "rate": rate,
            "rss": get_current_rss_mb() or 0.0,
            "bloom": self.seen.saturation(),
            "errors": self.network_errors,
            "robots": self.robots_blocks,
            "dupes": self.duplicate_content,
            "elapsed": elapsed,
            "status_breakdown": dict(self.stats.status),
            "content_types": dict(self.stats.types),
            "languages": dict(self.stats.languages),
            "top_domains": dict(self.stats.domains.most_common(10)),
        }

    def _tui_render(self, status=None):
        if not self.tui:
            return
        self.tui.update_state(self._get_state(status))
        self.tui.render()

    def _tui_pump(self):
        """Handle keyboard + render. Returns True if we should quit."""
        if not self.tui:
            return False
        key = self.tui.poll_key()
        if key == 'q':
            self._stop_reason = "user-quit"
            self._stop = True
            return True
        if key == 'p':
            self.paused = not self.paused
            self._tui_render()
        elif key == 'i':
            self.tui.toggle_info()
            self._tui_render()
        elif key == 'c':
            self.tui.clear_log()
            self._tui_render()
        return False

    # -- main loop ---------------------------------------------------------
    def run(self):
        if self.tui:
            self.tui.enter()
        try:
            self._run_loop()
        except KeyboardInterrupt:
            self._stop_reason = "ctrl-c"
            self.log.warn("keyboard interrupt")
        except Exception as e:
            self._stop_reason = f"error: {e}"
            import traceback
            self.log.error(f"crawler crashed: {e}")
            traceback.print_exc()
        finally:
            if self.tui:
                self.tui.exit()
            self._cleanup()
            self._final_report()

    def _run_loop(self):
        self.log.info(f"Gateway crawler v{VERSION}")
        self.log.info(f"  Output:         {self.output_path}")
        if self.storage == "json":
            self.log.info(f"  Output format:  JSON array (single .json file)")
        else:
            self.log.info(f"  Output format:  SQLite database")
        self.log.info(f"  Max pages:      {'infinity' if self.max_pages is None else self.max_pages}")
        self.log.info(f"  Max depth:      {self.max_depth}")
        self.log.info(f"  Frontier cap:   {self.frontier.maxsize:,}")
        self.log.info(f"  Bloom:          {self.seen.num_bits:,} bits ({self.seen.bytes_used()/1024:.1f} KB)")
        self.log.info(f"  RSS cap:        {self.max_memory_mb or 'infinity'} MB")
        self.log.info(f"  Sitemaps/Feeds: {'on' if self.use_sitemaps else 'off'}/{'on' if self.use_feeds else 'off'}")
        if self.notifier:
            self.log.info(f"  ntfy:           {self.notifier.url}  "
                          f"events={','.join(sorted(self.ntfy_events)) or '(none)'}"
                          f"  progress_every={self.ntfy_progress_every or 'off'}")
        self.log.info("-" * 60)

        # NEW: notify "started"
        self._notify(
            "started",
            f"Gateway crawler v{VERSION} started\n"
            f"seeds: {len(self.seeds)}\n"
            f"output: {self.output_path}\n"
            f"max_pages: {'inf' if self.max_pages is None else self.max_pages}\n"
            f"max_depth: {self.max_depth}",
            title="Gateway crawler started",
            priority=3,
            tags=["arrow_forward"],
        )

        self._discover_sitemaps()
        self._discover_feeds()

        mem_check_counter = 0

        while self.frontier and not self._stop:
            if self.tui and self.tui.enabled:
                if self._tui_pump():
                    break
                if self.paused:
                    self._tui_render("PAUSED")
                    time.sleep(0.15)
                    continue

            if self.max_pages is not None and self.page_count >= self.max_pages:
                self._stop_reason = "page-limit"
                break

            if not self._disk_ok():
                self.log.warn("disk space below 50 MB; stopping")
                self._stop_reason = "disk-full"
                break

            mem_check_counter += 1
            if self.max_memory_mb and mem_check_counter >= 10:
                mem_check_counter = 0
                cur = get_current_rss_mb()
                if cur is not None and cur > self.max_memory_mb:
                    self.log.warn(f"RSS {cur:.1f} MB > {self.max_memory_mb} MB; stopping")
                    self._stop_reason = "memory-limit"
                    break

            item = self.frontier.pop()
            if item is None:
                break
            url, depth, base_url = item

            if self._url_excluded(url):
                self.filtered += 1
                continue
            if self._domain_capped(url):
                continue
            if not self.ignore_robots and not self.robots.can_fetch(url):
                self.robots_blocks += 1
                if self.verbose_robots and get_domain(url) not in self._robots_explained:
                    self._robots_explained[get_domain(url)] = True
                    self.log.warn(f"robots blocks {url}")
                continue

            self._polite_wait(url)
            if self._stop:
                break

            if self.tui:
                self.tui.set_current(url)
                self._tui_render("FETCHING")

            self.log.info(f"[{self.page_count + 1}] {url}")
            result, _ = self.fetcher.fetch(url)
            self.stats.record(result)

            if result.error:
                self.log.warn(f"  error: {result.error}")
                self.network_errors += 1
                self.stats.record_fail(url, result.error)
                self._tui_render()
                continue
            if result.status == 304:
                self.log.debug(f"  304 not modified: {url}")
                self._tui_render()
                continue
            if result.status >= 400:
                self.network_errors += 1
                self.stats.record_fail(url, f"HTTP {result.status}")
                self._tui_render()
                continue

            # FIX (#3): don't write non-HTML responses as empty records.
            if result.meta is None:
                self.filtered += 1
                self.log.debug(f"  skipped (content-type: {result.content_type or 'unknown'})")
                self._tui_render()
                continue

            if self.language_filter:
                lang = (result.language or (result.meta or {}).get("lang") or "").lower()
                if lang and lang not in self.language_filter:
                    self.filtered += 1
                    self._tui_render()
                    continue

            obj, links, robots_meta = self._build_result(url, result)

            # respect noindex (token-aware to avoid matching "x-noindex")
            combined_robots = (robots_meta + " " + (result.x_robots_tag or "")).lower()
            robot_tokens = {t.strip() for t in re.split(r"[,;\s]+", combined_robots) if t.strip()}
            if self.respect_noindex and "noindex" in robot_tokens:
                self.noindex_skipped += 1
                self.log.debug(f"  noindex: {url}")
                self._tui_render()
                continue

            if obj["content_hash"]:
                if obj["content_hash"] in self.seen_content:
                    self.duplicate_content += 1
                    self._tui_render()
                    continue
                self.seen_content.add(obj["content_hash"])

            self._write(obj)
            self._domain_pages[get_domain(url)] += 1
            self.stats.domains[get_domain(url)] += 1
            if obj["lang"]:
                self.stats.languages[obj["lang"]] += 1

            size_kb = result.html_bytes / 1024.0
            self.log.info(f"  saved: {obj['t'][:60]} ({size_kb:.1f}KB)")

            # NEW: periodic progress notifications
            if (self.notifier
                    and "progress" in self.ntfy_events
                    and self.ntfy_progress_every > 0
                    and self.page_count % self.ntfy_progress_every == 0):
                pct = ""
                if self.max_pages:
                    pct = f" ({self.page_count*100.0/self.max_pages:.0f}%)"
                self._notify(
                    "progress",
                    f"Pages: {self.page_count}{pct}\n"
                    f"Queue: {len(self.frontier)}  |  "
                    f"Errors: {self.network_errors}  |  "
                    f"Robots blocks: {self.robots_blocks}\n"
                    f"Rate: {self.page_count / max(time.time()-self.stats.start,0.001) * 60:.1f}/min",
                    title=f"Progress: {self.page_count} pages",
                    priority=2,
                    tags=["bar_chart"],
                )

            # respect nofollow
            if "nofollow" in robot_tokens:
                links = []

            for href in links[: self.max_links_per_page]:
                absolute = normalize_url(href, result.final_url or url)
                if absolute:
                    self._enqueue(absolute, depth + 1, result.final_url or url)

            if self.page_count and self.page_count % 200 == 0:
                Checkpoint(self.checkpoint_path).save(self)
                sat = self.seen.saturation()
                if sat > 0.7:
                    self.log.warn(f"bloom filter {sat*100:.0f}% saturated "
                                  f"(increase --bloom-capacity)")

            self._tui_render()

        if self._stop_reason == "unknown":
            if self._stop: self._stop_reason = "interrupted"
            elif not self.frontier: self._stop_reason = "frontier-empty"
            else: self._stop_reason = "stopped"

    def _cleanup(self):
        try:
            self._writer.close()
        except Exception:
            pass
        try:
            if self.page_count and not self.dry_run:
                Checkpoint(self.checkpoint_path).save(self)
        except Exception:
            pass

    def _final_report(self):
        peak = get_peak_rss_mb()
        summary = self.stats.summary(self.page_count)
        self.log.info("-" * 60)
        self.log.info(f"stopped: {self._stop_reason}")
        for k, v in summary.items():
            self.log.info(f"  {k}: {v}")
        self.log.info(f"  robots_blocks: {self.robots_blocks}")
        self.log.info(f"  network_errors: {self.network_errors}")
        self.log.info(f"  duplicate_content: {self.duplicate_content}")
        self.log.info(f"  noindex_skipped: {self.noindex_skipped}")
        self.log.info(f"  filtered: {self.filtered}")
        self.log.info(f"  frontier_dropped: {self.frontier.dropped}")
        self.log.info(f"  bloom_saturation: {self.seen.saturation()*100:.1f}%")
        if peak is not None:
            self.log.info(f"  peak_rss_mb: {peak:.1f}")

        output_info = self.output_path
        validation_failed = False
        if self.storage == "json" and not self.dry_run:
            ok, reason = self._validate_output_file()
            if ok:
                self.log.info(f"  output_validation: OK (valid JSON array)")
                output_info += " (valid JSON array)"
            else:
                self.log.warn(f"  output_validation: FAILED ({reason})")
                output_info += f" (validation FAILED: {reason})"
                validation_failed = True
            self.log.info(f"  output_file: {self.output_path}")
        elif self.dry_run:
            self.log.info(f"  output_file: (dry run; nothing written)")
            output_info = "(dry run; nothing written)"
        else:
            self.log.info(f"  output_file: {self.output_path}")

        # NEW: notify "finished" — priority + tags depend on how we stopped.
        reason = self._stop_reason or ""
        if reason.startswith("error"):
            priority, tags, title = 5, ["rotating_light"], "Crawler crashed"
        elif validation_failed:
            priority, tags, title = 4, ["warning"], "Crawl finished (validation FAILED)"
        elif reason in ("interrupted", "ctrl-c", "user-quit"):
            priority, tags, title = 4, ["warning"], "Crawl interrupted"
        elif reason in ("disk-full", "memory-limit"):
            priority, tags, title = 4, ["warning"], f"Crawl stopped: {reason}"
        else:
            priority, tags, title = 3, ["checkered_flag"], "Crawl finished"

        self._notify(
            "finished",
            f"reason: {reason}\n"
            f"pages: {self.page_count}\n"
            f"errors: {self.network_errors}  duplicates: {self.duplicate_content}\n"
            f"elapsed: {summary['elapsed_seconds']}s  rate: {summary['pages_per_minute']}/min\n"
            f"output: {output_info}",
            title=title,
            priority=priority,
            tags=tags,
            click=self.output_path if self.storage == "json" and not self.dry_run else None,
        )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def load_config_file(path):
    if not path:
        return {}
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        print(f"[WARN] config load failed: {e}", file=sys.stderr)
        return {}


def parse_args():
    p = argparse.ArgumentParser(
        description="Crawl the web into an XPDevs Gateway index (bounded RAM, TUI).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)

    p.add_argument("--seed", action="append", default=[], help="Seed URL (repeatable).")
    p.add_argument("--seed-file", default=None,
                   help="File with one seed URL per line.")
    p.add_argument("--max-pages", type=int, default=None,
                   help="Stop after N pages (omit for infinite).")
    p.add_argument("--forever", action="store_true",
                   help="Ignore --max-pages; run until frontier empties.")
    p.add_argument("--output", default=DEFAULT_OUTPUT,
                   help=f"Output path (default: {DEFAULT_OUTPUT}).")
    p.add_argument("--storage", choices=("json", "sqlite"), default="json",
                   help="Output format (default: json array).")
    p.add_argument("--delay", type=float, default=DEFAULT_DELAY,
                   help=f"Per-host delay seconds (default: {DEFAULT_DELAY}).")
    p.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT,
                   help=f"Request timeout seconds (default: {DEFAULT_TIMEOUT}).")
    p.add_argument("--user-agent", default=DEFAULT_USER_AGENT,
                   help="User-Agent string.")
    p.add_argument("--config", default=None,
                   help="JSON config file with defaults (CLI overrides).")
    p.add_argument("--version", action="version",
                   version=f"gateway_crawler {VERSION}")

    p.add_argument("--same-host", action="store_true",
                   help="Only follow links on the seed domain(s).")
    p.add_argument("--exclude-domain", action="append", default=[],
                   help="Exclude domains matching this regex (repeatable).")
    p.add_argument("--exclude-url", action="append", default=[],
                   help="Exclude URLs matching this regex (repeatable).")
    p.add_argument("--language", action="append", default=[],
                   help="Restrict to these languages (ISO codes, repeatable).")

    p.add_argument("--max-depth", type=int, default=DEFAULT_MAX_DEPTH,
                   help=f"Max crawl depth (default: {DEFAULT_MAX_DEPTH}).")
    p.add_argument("--max-per-domain", type=int, default=DEFAULT_MAX_PER_DOM,
                   help=f"Max pages per domain (default: {DEFAULT_MAX_PER_DOM}).")
    p.add_argument("--max-links-per-page", type=int, default=DEFAULT_MAX_LINKS,
                   help=f"Max links extracted per page (default: {DEFAULT_MAX_LINKS}).")
    p.add_argument("--max-queue-size", type=int, default=DEFAULT_MAX_QUEUE,
                   help=f"Frontier capacity (default: {DEFAULT_MAX_QUEUE}).")
    p.add_argument("--bloom-capacity", type=int, default=DEFAULT_BLOOM_CAP,
                   help=f"Bloom filter capacity (default: {DEFAULT_BLOOM_CAP}).")
    p.add_argument("--bloom-fp", type=float, default=DEFAULT_BLOOM_FP,
                   help=f"Bloom false-positive rate (default: {DEFAULT_BLOOM_FP}).")
    p.add_argument("--max-page-bytes", type=int, default=DEFAULT_PAGE_BYTES,
                   help=f"Max bytes per page (default: {DEFAULT_PAGE_BYTES}).")
    p.add_argument("--chunk-size", type=int, default=DEFAULT_CHUNK,
                   help=f"Streaming chunk size (default: {DEFAULT_CHUNK}).")
    p.add_argument("--max-memory-mb", type=float, default=None,
                   help="Abort if current RSS exceeds this many MB.")
    p.add_argument("--max-retries", type=int, default=DEFAULT_MAX_RETRIES,
                   help=f"Retry attempts on transient failures (default: {DEFAULT_MAX_RETRIES}).")

    p.add_argument("--ignore-robots", action="store_true",
                   help="Skip robots.txt checks (own sites only).")
    p.add_argument("--verbose-robots", action="store_true",
                   help="Explain robots.txt blocks.")
    p.add_argument("--no-respect-noindex", action="store_true",
                   help="Ignore noindex / X-Robots-Tag rules.")

    p.add_argument("--use-sitemaps", action="store_true",
                   help="Discover and parse sitemap.xml / sitemap.txt.")
    p.add_argument("--use-feeds", action="store_true",
                   help="Discover and parse RSS/Atom feeds.")

    p.add_argument("--checkpoint", default=DEFAULT_CHECKPOINT,
                   help=f"Checkpoint file (default: {DEFAULT_CHECKPOINT}).")
    p.add_argument("--resume", action="store_true",
                   help="Resume from checkpoint if present.")

    p.add_argument("--debug", action="store_true",
                   help="Enable detailed tracing and write every event to crawl_debug.log.")
    p.add_argument("--debug-log", default="crawl_debug.log",
                   help="Debug log path (used with --debug; default: crawl_debug.log).")
    p.add_argument("--quiet", action="store_true",
                   help="Suppress INFO/DEBUG logs (TUI still shows progress).")
    p.add_argument("--log-format", choices=("text", "json"), default="text",
                   help="Log format when TUI is disabled (default: text).")
    p.add_argument("--dry-run", action="store_true",
                   help="Fetch and parse but do not write output.")
    p.add_argument("--no-tui", action="store_true",
                   help="Disable full-screen TUI (logs to stderr).")
    p.add_argument("--refresh-rate", type=float, default=TUI.REFRESH_DEFAULT,
                   help=f"TUI refresh interval in seconds (default: {TUI.REFRESH_DEFAULT}).")

    # NEW: ntfy flags. No token needed — just a topic.
    ntfy_group = p.add_argument_group("ntfy.sh notifications")
    ntfy_group.add_argument(
        "--ntfy-topic", default="", metavar="TOPIC",
        help="Push notifications to https://ntfy.sh/<TOPIC>. "
             "Pass a full URL (https://…) to use a self-hosted server. "
             "Requires no account or token. Disabled by default.")
    ntfy_group.add_argument(
        "--ntfy-events", default=DEFAULT_NTFY_EVENTS, metavar="LIST",
        help=f"Comma-separated events to notify: started,progress,finished. "
             f"Default: {DEFAULT_NTFY_EVENTS}")
    ntfy_group.add_argument(
        "--ntfy-progress-every", type=int, default=0, metavar="N",
        help="Send a progress notification every N pages. "
             "0 disables progress notifications (default). "
             "Only takes effect if 'progress' is in --ntfy-events.")
    return p.parse_args()


def main():
    a = parse_args()

    cfg = load_config_file(a.config)

    seeds = list(a.seed)
    if a.seed_file:
        try:
            with open(a.seed_file, encoding="utf-8") as f:
                seeds.extend(line.strip() for line in f
                             if line.strip() and not line.startswith("#"))
        except Exception as e:
            print(f"[ERROR] cannot read seed file: {e}", file=sys.stderr)
            sys.exit(2)
    if not seeds:
        print("[ERROR] provide at least one --seed or --seed-file", file=sys.stderr)
        sys.exit(2)

    if a.storage == "json" and not a.output.lower().endswith(".json"):
        print(f"[WARN] --output '{a.output}' does not end with '.json'. "
              f"The file will still contain a valid JSON array, but the "
              f"extension is misleading.", file=sys.stderr)

    max_pages = None if (a.forever or not a.max_pages) else a.max_pages
    if a.forever and a.max_pages:
        print("[WARN] --forever overrides --max-pages", file=sys.stderr)

    tui = None
    if not a.no_tui and not a.debug:
        tui = TUI(refresh_rate=a.refresh_rate)
        if not tui.enabled:
            tui = None

    # NEW: build ntfy notifier (if requested).
    notifier = None
    ntfy_events = []
    if a.ntfy_topic:
        try:
            notifier = NtfyNotifier(a.ntfy_topic)
            ntfy_events = [e.strip().lower() for e in a.ntfy_events.split(",")
                           if e.strip()]
        except Exception as e:
            print(f"[WARN] ntfy disabled: {e}", file=sys.stderr)

    crawler = GatewayCrawler(
        seeds=seeds,
        max_pages=max_pages,
        delay=a.delay,
        output_path=a.output,
        user_agent=a.user_agent,
        timeout=a.timeout,
        same_host_only=a.same_host,
        verbose=True,
        quiet=a.quiet,
        max_queue_size=a.max_queue_size,
        bloom_capacity=a.bloom_capacity,
        bloom_fp=a.bloom_fp,
        max_page_bytes=a.max_page_bytes,
        chunk_size=a.chunk_size,
        max_memory_mb=a.max_memory_mb,
        ignore_robots=a.ignore_robots,
        verbose_robots=a.verbose_robots,
        max_depth=a.max_depth,
        max_per_domain=a.max_per_domain,
        max_links_per_page=a.max_links_per_page,
        exclude_domains=a.exclude_domain,
        exclude_patterns=a.exclude_url,
        use_sitemaps=a.use_sitemaps,
        use_feeds=a.use_feeds,
        storage=a.storage,
        checkpoint_path=a.checkpoint,
        resume=a.resume,
        dry_run=a.dry_run,
        max_retries=a.max_retries,
        log_fmt=a.log_format,
        language_filter=a.language,
        respect_noindex=not a.no_respect_noindex,
        tui=tui,
        debug_log=(a.debug_log if a.debug else None),
        # NEW: ntfy
        ntfy_notifier=notifier,
        ntfy_events=ntfy_events,
        ntfy_progress_every=a.ntfy_progress_every,
    )
    crawler.run()
    crawler.log.close()


if __name__ == "__main__":
    main()
