This folder contains the automatic test suite for openhatd.

These tests are not intended to be run with -t as this switch mostly validates the syntactic correctness of a configuration file only.

Instead they use Test ports to check whether the behaviour of the configuration is as expected.

These tests are required to terminate by themselves, either by a failing test or by containing at least one timely executed Test port that uses ExitAfterTest = true.

Succeeding tests will cause openhatd to exit with code 0. This property can be used to detect test failures.
