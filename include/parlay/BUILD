licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "alloc",
    hdrs = ["alloc.h"],
    deps = [
        ":type_traits",
        ":utilities",
        "//parlay/internal:block_allocator",
        "//parlay/internal:memory_size",
        "//parlay/internal:pool_allocator",
    ],
)

cc_library(
    name = "delayed",
    hdrs = ["delayed.h"],
    deps = [
        ":delayed_sequence",
        ":range",
        ":type_traits",
        "//parlay/internal/delayed:filter",
        "//parlay/internal/delayed:filter_op",
        "//parlay/internal/delayed:flatten",
        "//parlay/internal/delayed:map",
        "//parlay/internal/delayed:scan",
        "//parlay/internal/delayed:terminal",
        "//parlay/internal/delayed:zip",
        "//parlay/internal:sequence_ops",
    ],
)

cc_library(
    name = "delayed_sequence",
    hdrs = ["delayed_sequence.h"],
    deps = [
        ":utilities",
    ],
)

cc_library(
    name = "hash_table",
    hdrs = ["hash_table.h"],
    deps = [
        ":delayed_sequence",
        ":parallel",
        ":primitives",
        ":sequence",
        ":slice",
        ":utilities",
    ],
)

cc_library(
    name = "io",
    hdrs = ["io.h"],
    deps = [
        ":primitives",
        ":sequence",
        ":slice",
        "//parlay/internal:file_map",
    ],
)

cc_library(
    name = "monoid",
    hdrs = ["monoid.h"],
    deps = [
        ":portability",
        ":type_traits",
    ]
)

cc_library(
    name = "parallel",
    hdrs = ["parallel.h"],
    deps = [
        ":scheduler",
        "//parlay/internal/scheduler_plugins:sequential",
        "//parlay/internal:work_stealing_job",
    ],
)

cc_library(
    name = "portability",
    hdrs = ["portability.h"],
)

cc_library(
    name = "primitives",
    hdrs = ["primitives.h"],
    deps = [
        ":delayed",
        ":delayed_sequence",
        ":monoid",
        ":parallel",
        ":random",
        ":range",
        ":relocation",
        ":sequence",
        ":slice",
        ":type_traits",
        ":utilities",
        "//parlay/internal:block_delayed",
        "//parlay/internal:collect_reduce",
        "//parlay/internal:counting_sort",
        "//parlay/internal:group_by",
        "//parlay/internal:integer_sort",
        "//parlay/internal:heap_tree",
        "//parlay/internal:merge",
        "//parlay/internal:merge_sort",
        "//parlay/internal:sample_sort",
        "//parlay/internal:sequence_ops",
    ],
)

cc_library(
    name = "random",
    hdrs = ["random.h"],
    deps = [
        ":delayed_sequence",
        ":parallel",
        ":range",
        ":sequence",
        ":slice",
        ":utilities",
        "//parlay/internal:counting_sort",
    ],
)

cc_library(
    name = "range",
    hdrs = ["range.h"],
    deps = [
        ":type_traits",
    ]
)

cc_library(
    name = "relocation",
    hdrs = ["relocation.h"],
    deps = [
        "//parlay/internal:debug_uninitialized",
        ":parallel",
        ":range",
        ":type_traits",
        ":utilities",
    ],
)

cc_library(
    name = "scheduler",
    hdrs = ["scheduler.h"],
    deps = [
        "//parlay/internal:atomic_wait",
        "//parlay/internal:work_stealing_deque",
        "//parlay/internal:work_stealing_job",
    ],
)

cc_library(
    name = "sequence",
    hdrs = ["sequence.h"],
    deps = [
        ":alloc",
        ":parallel",
        ":portability",
        ":range",
        ":slice",
        ":type_traits",
        ":utilities",
        "//parlay/internal:debug_uninitialized",
        "//parlay/internal:sequence_base",
    ],
)

cc_library(
    name = "slice",
    hdrs = ["slice.h"],
    deps = [
        ":range",
    ],
)

cc_library(
    name = "thread_specific",
    hdrs = ["thread_specific.h"],
    deps = [
        ":portability",
        ":range",
        ":type_traits",
        "//parlay/internal:thread_id_pool",
    ],
)

cc_library(
    name = "type_traits",
    hdrs = ["type_traits.h"],
)

cc_library(
    name = "utilities",
    hdrs = ["utilities.h"],
    deps = [
        ":portability",
        ":type_traits",
        "//parlay/internal:debug_uninitialized",
    ],
)

cc_library(
    name = "worker_specific",
    hdrs = ["worker_specific.h"],
    deps = [
        ":parallel",
        ":range",
        ":type_traits",
        ":utilities",
    ],
)
