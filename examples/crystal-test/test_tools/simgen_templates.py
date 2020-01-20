TESTBED_TEMPLATE = \
r"""
{
    "name" : "Crystal Test",
    "description" : "Run Glossy for {{ duration_minutes }} minutes",
    "ts_init" : "{{ ts_init }}",
    "duration" : {{ duration_seconds }},
    "image" : {
        "hardware" : "evb1000",
        "file":    "{{ abs_bin_path }}",
        "target":  {{ targets }}
    },
    "logs": 0
}
"""
