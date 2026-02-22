# Code Style Examples

## Example: Over-Engineering vs Simplicity

*While this example is Python, the same anti-patterns appear in any language: unnecessary intermediate representations, excessive structure, encode-then-decode round-trips.*

### Bad - Over-engineered

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import List, Tuple, Optional


@dataclass(frozen=True)
class Node:
    delay: int
    children: List["Node"]


def parse_seq(s: str, i: int = 0) -> Tuple[List[Node], int]:
    """
    Parse a sequence of nodes until end-of-string or a closing ']'.
    Returns (nodes, next_index).
    """
    nodes: List[Node] = []
    n = len(s)

    while i < n and s[i] != "]":
        ch = s[i]
        if not ch.isdigit():
            raise ValueError(f"Expected digit at pos {i}, got {ch!r}")
        delay = int(ch)
        i += 1

        children: List[Node] = []
        if i < n and s[i] == "[":
            i += 1
            children, i = parse_seq(s, i)
            if i >= n or s[i] != "]":
                raise ValueError(f"Missing closing ']' for '[' opened earlier (pos {i})")
            i += 1

        nodes.append(Node(delay=delay, children=children))

    return nodes, i


def parse_spec(s: str) -> List[Node]:
    nodes, i = parse_seq(s, 0)
    if i != len(s):
        raise ValueError(f"Unexpected ']' at pos {i}")
    return nodes


def node_to_spec(node: Node) -> str:
    if not node.children:
        return str(node.delay)
    return f"{node.delay}[{''.join(node_to_spec(c) for c in node.children)}]"


def seq_to_spec(seq: List[Node]) -> str:
    return "".join(node_to_spec(n) for n in seq)


def spawn_child(script_path: str, node_spec: str, hold: float, name: str) -> subprocess.Popen:
    env = os.environ.copy()
    env["TREE_NODE_NAME"] = name
    argv = [sys.executable, script_path, "--worker", node_spec, "--hold", str(hold)]
    return subprocess.Popen(argv, env=env)


def worker_run(script_path: str, node: Node, hold: float) -> int:
    name = os.environ.get("TREE_NODE_NAME", "")
    time.sleep(node.delay)

    procs: List[subprocess.Popen] = []
    for idx, child in enumerate(node.children):
        child_spec = node_to_spec(child)
        child_name = f"{name}.{idx}" if name else str(idx)
        procs.append(spawn_child(script_path, child_spec, hold, child_name))

    time.sleep(hold)

    deadline = time.time() + 5.0
    for p in procs:
        timeout = max(0.0, deadline - time.time())
        try:
            p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            pass

    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("spec", help="Tree spec, e.g. '3[12[3]4]5'")
    ap.add_argument("--hold", type=float, default=20.0)
    ap.add_argument("--worker", action="store_true")
    args = ap.parse_args()

    script_path = os.path.abspath(sys.argv[0])

    if args.worker:
        seq = parse_spec(args.spec)
        if len(seq) != 1:
            raise SystemExit("Worker mode expects a single node spec.")
        return worker_run(script_path, seq[0], args.hold)

    seq = parse_spec(args.spec)
    root_name = "root"
    procs: List[subprocess.Popen] = []
    for idx, node in enumerate(seq):
        node_spec = node_to_spec(node)
        name = f"{root_name}.{idx}"
        procs.append(spawn_child(script_path, node_spec, args.hold, name))

    time.sleep(args.hold)

    deadline = time.time() + 5.0
    for p in procs:
        timeout = max(0.0, deadline - time.time())
        try:
            p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

### Good - Minimal

```python
import os, subprocess, sys

def parse(spec: str):
    """parse `'4[22[3]][4]'` into a generator of `4, '22[3]', '4'`"""
    buffer = ""
    for ch in spec:
        if buffer:
            buffer += ch
            if buffer.count("[") == buffer.count("]"):
                assert buffer[1:-1], "[] is empty"
                yield buffer[1:-1]
                buffer = ""
        elif ch == "[":
            buffer += ch
        else:
            yield int(ch)

me = os.path.abspath(sys.argv[0])
spec = sys.argv[1]

ps: list[subprocess.Popen] = []
for i in parse(spec):
    if type(i) == int:
        ps.append(subprocess.Popen(["sleep", f"{i}"]))
    else:
        ps.append(subprocess.Popen([sys.executable, me, str(i)]))
for p in ps:
    p.wait()
```
