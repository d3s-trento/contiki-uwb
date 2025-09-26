TESTBED_TEMPLATE = \
r"""
{
    "name" : "Weaver Test",
    "description" : "Run Weaver for {{ duration_minutes }} minutes",
    "start_time" : "{{ start_time }}",
    "duration" : {{ duration_seconds }},
    "island": "DEPT",
    "binaries" : {
        "hardware": "evb1000",
        "bin_file": "< path-to-binary >",
        "targets":  "< list-of-targets >"
    },
    "logs" : 0
}
"""
