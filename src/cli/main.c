/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 */

 // core/cli/main.c
#include <sigma.core/module.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    sigma_module_init_all();

    printf("Welcome to Anvil\n");
    //  TODO: Update version from build system
    printf("Version 1.0 · December 2025 · Quantum Override\n");

    sigma_module_shutdown_all();
    return 0;
}