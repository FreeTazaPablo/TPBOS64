#ifndef ATA_H
#define ATA_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/* Lee 1 sector (512 bytes) del disco secundario (hdb) en buf */
void ata_read_sector (uint32_t lba, uint8_t *buf);

/* Escribe 1 sector (512 bytes) desde buf al disco secundario (hdb) */
void ata_write_sector(uint32_t lba, const uint8_t *buf);

#endif
