#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

int acpi_init(void);
int acpi_poweroff(void);
int acpi_reboot(void);
int acpi_available(void);

#endif
