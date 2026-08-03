from pytest_embedded_idf import IdfDut


def test_unity_app(dut: IdfDut) -> None:
    """Wait for the firmware's batch Unity run to finish.

    expect_unity_test_output() parses every per-case ``...:PASS/FAIL`` line
    into dut.testsuite (also exported to JUnit XML), then waits for the
    summary line. A hang fails in 120 s instead of blocking for the full
    CI timeout, and the assertion reports exactly which cases failed.
    """
    dut.expect_unity_test_output(timeout=120)
    failed = [case.name for case in dut.testsuite.testcases if case.result == 'FAIL']
    assert not failed, f'Unity test cases failed: {failed}'
    assert dut.testsuite.attrs['failures'] == 0