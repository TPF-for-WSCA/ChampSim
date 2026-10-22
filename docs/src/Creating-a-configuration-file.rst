.. _Creating_Config:

================================================
Creating a Configuration File
================================================

The configuration file is a central fixture of the ChampSim build system.
An example configuration file (champsim_config.json) is given in the root of the repository, but it is large and unwieldy.
Your configuration file will likely be much smaller.
This page will walk you through many of the features of the ChampSim configuration system.

The configuration file is given as an input to the configuration script as below::

    ./config.sh my_config.json

In fact, the configuration file is entirely optional.
Not specifying any file will configure ChampSim with a default configuration.

-------------------------------
Your first configuration file
-------------------------------

All ChampSim configuration files are JSON files.
We can start with the most trivial of configuration files.::

    {
    }

This would specify a default configuration.
But, this is not frequently useful.
Let's change the branch predictor that ChampSim uses.
Legal values for the ``branch_predictor`` key are directory names under the ``branch/`` directory, or valid paths.
The same is true for specifying BTBs in the core and both prefetchers and replacement policies in the cache.::

    {
        "branch_predictor": "perceptron"
    }

This configuration file will configure ChampSim with a default configuration, but with a perceptron branch predictor instead.
We can take this further and specify many things about our single-core system.::

    {
        "branch_predictor": "perceptron",

        "rob_size": 226, "lq_size": 90, "sq_size": 85,
        "fetch_width": 6, "decode_width": 6, "lq_width": 2, "sq_width": 1,

        "decode_latency": 3, "execute_latency": 2
    }

Each of these options will specify something about our core. Next, we'll specify some of our caches.

---------------------
Cache Configuration
---------------------

We can begin by changing some features of our L1 data cache.
We can specify its size based on the block size, the number of sets, and the number of ways.
The following configuration file specifies a 64 KiB L1 data cache.::

    {
        "block_size": 64,
        "L1D": { "sets": 256, "ways": 4 }
    }

Note that 64 is the default block size, specified here for clarity.
The default prefetcher is the do-nothing prefetcher, and the default replacement policy is LRU, but they can be specified, too.::

    {
        "L1D": {
            "sets": 256, "ways": 4,
            "prefetcher": "next_line",
            "replacement": "../../my/repl/path"
        }
    }

Specifying a cache this way will create an identical L1D for each core in the configuration.
So far, we've only handled the single-core case.

Uneven Block Size instruction cache
-----------------------------------

The MICRO-24 Uneven Block Size instruction cache (called VCL in the original implementation)
can be selected on an L1I with the ``ubs`` flag. The conventional cache remains
the default. For example::

    {
        "L1I": {
            "ubs": true,
            "prefetcher": "fdip"
        }
    }

With only the flag specified, UBS uses the paper configuration: a direct-mapped
predictor with one entry per cache set, 16 ways sized ``4, 4, 8, 8, 8, 12, 12,
16, 24, 32, 36, 36, 52, 64, 64, 64`` bytes, and four candidate ways for its
modified-LRU replacement. Experiments can override ``ubs_way_sizes`` and
``ubs_candidate_ways``; custom way sizes must be sorted, positive, and end in a
64-byte way. UBS currently requires the standard 64-byte cache line and always
uses its internal modified-LRU policy rather than the configured replacement module.
Specify ``sets`` explicitly when scaling UBS capacity; a generic cache ``size``
is converted to sets using the conventional ``ways`` geometry before UBS replaces
those ways with its uneven organization.

UBS partial misses are reported as misses to the configured instruction
prefetcher, and appear as ``partial_miss`` in JSON cache statistics and as
``UBS PARTIAL MISSES`` in the plain-text statistics.

--------------------------
Multi-core configurations
--------------------------

Multi-core simulations can be enabled by specifying the ``num_cores`` key.::

    {
        "num_cores": 2
    }

This will create two identical cores with default configurations.
This can be combined with the other specifications that we have made so far to create identical cores.::

    {
        "num_cores": 2,
        "branch_predictor": "perceptron",

        "rob_size": 190,

        "L1D": {
            "sets": 256, "ways": 4,
            "prefetcher": "next_line",
            "replacement": "drrip"
        }
    }

-----------------------
Heterogeneous systems
-----------------------

ChampSim supports a variety of system configurations, including systems that are not homogeneous.
For example, we could create two cores with different ROB sizes.
Specify all cores in a list under the ``ooo_cpu`` key.::

    {
        "num_cores": 2,
        "ooo_cpu": [
            { "rob_size": 120 },
            { "rob_size": 240 }
        ]
    }

Each object in the ``ooo_cpu`` list specifies one core, and takes all of the options that we have discussed so far.::


    {
        "num_cores": 2,
        "ooo_cpu": [
            {
                "branch_predictor": "bimodal",
                "rob_size": 120, "lq_size": 90,
                "L1D": { "prefetcher": "next_line" }
            },
            {
                "branch_predictor": "gshare",
                "rob_size": 240, "lq_size": 70,
                "L1D": { "prefetcher": "no" }
            }
        ]
    }

Each cache object can also be specified in a list under the ``caches`` key.
These caches can then be referred to by their ``name`` key.
In the following configuration, each core has a distinct L1 cache.::

    {
        "num_cores": 2,
        "ooo_cpu": [
            { "L1D": "cacheA" },
            { "L1D": "cacheB" }
        ],
        "caches": [
            { "name": "cacheA", "replacement": "lru" },
            { "name": "cacheB", "replacement": "srrip" }
        ]
    }

The configuration script will make every attempt to assign defaults to objects, but it may not be able to do so for.
In the following configuration, cores 0 and 1 are attached to ``llcA`` and cores 2 and 3 are attached to ``llcB``.
The script is able to assign LLC-like defaults to each of the caches specified under ``"caches"``::

    {
        "num_cores": 4,
        "ooo_cpu": [
            { "L2C": { "lower_level": "llcA"} },
            { "L2C": { "lower_level": "llcB"} }
        ],
        "caches": [
            { "name": "llcA" },
            { "name": "llcB" }
        ]
    }

However, in the following, the script will not be able to assign defaults to all caches, and you may need to specify additional parameters::

    {
        "num_cores": 8,
        "ooo_cpu": [
            { "L2C": { "lower_level": "llcA"} },
            { "L2C": { "lower_level": "llcB"} }
        ],
        "caches": [
            { "name": "llcA", "lower_level": "L4C" },
            { "name": "llcB", "lower_level": "L4C" },
            { "name": "L4C" }
        ]
    }
