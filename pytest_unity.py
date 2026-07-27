from pytest_embedded_idf import IdfDut


def test_unity_app(dut: IdfDut) -> None:
    """Wait for the firmware's unity_run_all_tests() batch run to finish
    and assert that no Unity test case failed."""
    dut.expect_unity_test_output(timeout=300)