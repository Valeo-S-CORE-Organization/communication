# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0

import time
import logging

logger = logging.getLogger(__name__)

def test_generic_typed_interaction(sut):
    """
    This integration test verifies that a GenericSkeleton can successfully
    communicate with a standard, typed Proxy.

    It launches a single application in two modes:
    1. Provider mode, using GenericSkeleton.
    2. Consumer mode, using a typed Proxy.

    The test passes if the consumer receives and validates all data sent
    by the provider without crashing.
    """
    app_root = "/opt/generic_typed_interaction_app/"
    app_bin = "./bin/generic_typed_interaction_app"
    config_path = "./etc/mw_com_config.json"
    logging_config_path = "./etc/logging.json"

    provider_cmd = f"{app_bin} --mode provider --service_instance_manifest {config_path}"
    consumer_cmd = f"{app_bin} --mode consumer --service_instance_manifest {config_path}"

    logger.info(f"Starting provider: {provider_cmd} in {app_root}")
    with sut.start_process(provider_cmd, cwd=app_root) as provider:
        # Give the provider a moment to initialize and offer the service
        # to prevent a race condition where the consumer starts too quickly.
        time.sleep(2)

        logger.info(f"Starting consumer: {consumer_cmd} in {app_root}")
        with sut.start_process(consumer_cmd, cwd=app_root) as consumer:
            # Wait for the consumer to finish. It will exit with 0 on success
            # or a non-zero code (or crash) on failure.
            consumer_exit_code = consumer.wait_for_exit(timeout=30)
            assert consumer_exit_code == 0, f"Consumer process failed with exit code {consumer_exit_code}, indicating a communication error or data mismatch."

            # The provider should also exit cleanly after sending its samples.
            provider_exit_code = provider.wait_for_exit(timeout=30)
            assert provider_exit_code == 0, f"Provider process failed with exit code {provider_exit_code}."