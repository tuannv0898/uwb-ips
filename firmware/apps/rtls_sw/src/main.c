/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include "os/mynewt.h"
#include "mesh/mesh.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "hal/hal_gpio.h"
#include "bsp/bsp.h"
#include "shell/shell.h"

#include "rtls_sw/ble_mesh.h"

void handler(void *arg){
    console_printf("BTN 1 click\n");
}

int
main(int argc, char **argv)
{

    /* Initialize OS */
    sysinit();

    hal_gpio_init_in(BUTTON_2, HAL_GPIO_PULL_UP);
    hal_gpio_irq_init(BUTTON_2, handler, NULL, HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_UP);
    hal_gpio_irq_enable(BUTTON_2);

    // ble_mesh_init();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}
