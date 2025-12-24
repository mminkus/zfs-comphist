# zfs-comphist

## Intro

`zfs-comphist` is a read-only ZFS analysis tool that walks datasets and reports block-level compression usage by algorithm.

It answers a simple question ZFS does not expose natively:
*what compression is actually in use on disk?*

## Why This Tool Exists

ZFS exposes a single aggregate metric (`compressratio`) to describe compression efficiency. While useful, it hides how that ratio is achieved and which compression algorithms are actually in use across a dataset or pool — especially over long-lived filesystems that span multiple ZFS eras.

`zfs-comphist` answers questions that ZFS does not natively expose:

- How much data is stored with `off`, `lzjb`, `gzip`, `lz4`, `zstd`, etc?
- What percentage of blocks use each algorithm?
- How much space could potentially be recovered by rewriting old data?
- Why do two datasets with identical logical contents have different space usage?

## Safety and Read-Only Operation

`zfs-comphist` is strictly read-only.

- No data blocks are modified
- No metadata is written
- No transactions are opened
- No pool state is changed

The tool inspects block pointers and metadata only, using the same
read-only traversal mechanisms as `zdb`.

It can be run safely on mounted pools. For maximum consistency, snapshots
are recommended when analyzing actively changing datasets.

---

## Example: Legacy Dataset vs Rewritten Dataset

### Original dataset on a long-lived pool (Luna)

This dataset contains data written over many years, spanning multiple ZFS defaults and compression policies:

```console
$ zfs-comphist luna/local/home/minecraft/TPPIServer-world --allow-live

Compression   Blocks     Block_%   Logical_B     Physical_B     Allocated_B  Ratio
------------------------------------------------------------------------------------
off                  3858    23.69      483269632      483269632      484089856    1.00
lzjb                12358    75.90     1561916416      752921600      776491008    2.07
lz4                    66     0.41        1033728         258048         516096    4.01
------------------------------------------------------------------------------------
total               16282   100.00     2046219776     1236449280     1261096960    1.65
holes: 40115
embedded blocks: 3 (logical bytes: 1536)
```

---

### Same Data After Migration to a New Pool (Nexus)

The same dataset, copied to a new pool using `zfs send | recv`, is fully rewritten under modern compression defaults:

```console
$ zfs-comphist nexus/local/home/minecraft/TPPIServer-world --allow-live

Compression   Blocks     Block_%   Logical_B     Physical_B     Allocated_B  Ratio
------------------------------------------------------------------------------------
off              1163     7.14      141661696      141661696      142028800    1.00
lz4               534     3.28       57261568        2019328        4038656   28.36
zstd            14583    89.58     1896464896      953085952      953085952    1.99
------------------------------------------------------------------------------------
total           16280   100.00     2095388160     1096766976     1099153408    1.91
```

**Key observations:**

- Nearly 90% of blocks are now compressed with `zstd`
- `off` blocks dropped from ~23% → ~7%

ZFS's own metrics confirm this improvement:

```console
$ zfs get compressratio nexus/local/home/minecraft
compressratio  1.86x
```

Compared to the original replica on Luna:

```console
$ zfs get compressratio luna/replica/nexus/home/minecraft
compressratio  1.64x
```

---

## What This Demonstrates

- ZFS never recompresses existing blocks
- Legacy pools accumulate historical inefficiencies
- `zfs send | recv` acts as implicit garbage collection
- Modern compression (especially `zstd`) provides measurable gains
- `compressratio` alone cannot explain why datasets differ
- Block-level accounting makes the difference obvious

---

## Why This Tool Is Useful

`zfs-comphist` provides visibility that ZFS itself does not:

- Identify datasets worth rewriting
- Quantify gains from migrations
- Validate compression policy changes
- Understand space usage on heterogeneous pools
- Explain discrepancies between logical and physical usage

It is especially valuable for:

- Long-lived pools
- Upgraded systems
- Migration planning
- Capacity analysis
- ZFS archaeology 🙂

## Feedback

Ideas, suggestions, and feedback are welcome.

If this proves broadly useful, parts of this functionality may be a good
candidate for inclusion in existing ZFS tooling such as `zdb`.
