/*
 * Linker скрипт за STM32H723ZG (1 MB flash), CNC контролер.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 *
 * Карта на паметта (RM0468, настройка по подразбиране на TCM/AXI):
 *   ITCMRAM  0x00000000   64 KB  бърз код
 *   DTCMRAM  0x20000000  128 KB  стек и бързи данни (НЕ е достъпна за DMA)
 *   RAM_D1   0x24000000  320 KB  AXI SRAM: .data, .bss, heap (достъпна за DMA)
 *   RAM_D2   0x30000000   32 KB  SRAM1/2: буфери за DMA (.dma_buffer)
 *   RAM_D3   0x38000000   16 KB  SRAM4
 */

ENTRY(Reset_Handler)

_Min_Heap_Size  = 0x2000;   /* 8 KB  */
_Min_Stack_Size = 0x4000;   /* 16 KB */

MEMORY
{
  FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  ITCMRAM  (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  DTCMRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  RAM_D1   (xrw) : ORIGIN = 0x24000000, LENGTH = 320K
  RAM_D2   (xrw) : ORIGIN = 0x30000000, LENGTH = 32K
  RAM_D3   (xrw) : ORIGIN = 0x38000000, LENGTH = 16K
}

/* стекът е в DTCM: най-бърз достъп, без кеш */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text) *(.text*)
    *(.glue_7) *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata) *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { . = ALIGN(4); *(.ARM.extab* .gnu.linkonce.armextab.*) . = ALIGN(4); } >FLASH
  .ARM : { . = ALIGN(4); __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; . = ALIGN(4); } >FLASH

  .preinit_array : { . = ALIGN(4); PROVIDE_HIDDEN (__preinit_array_start = .); KEEP (*(.preinit_array*)) PROVIDE_HIDDEN (__preinit_array_end = .); . = ALIGN(4); } >FLASH
  .init_array : { . = ALIGN(4); PROVIDE_HIDDEN (__init_array_start = .); KEEP (*(SORT(.init_array.*))) KEEP (*(.init_array*)) PROVIDE_HIDDEN (__init_array_end = .); . = ALIGN(4); } >FLASH
  .fini_array : { . = ALIGN(4); PROVIDE_HIDDEN (__fini_array_start = .); KEEP (*(SORT(.fini_array.*))) KEEP (*(.fini_array*)) PROVIDE_HIDDEN (__fini_array_end = .); . = ALIGN(4); } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data) *(.data*)
    *(.RamFunc) *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM_D1 AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss) *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM_D1

  /* heap след .bss в RAM_D1 */
  ._user_heap :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = ALIGN(8);
  } >RAM_D1

  /* проверка, че стекът се побира в DTCM */
  ._user_stack (NOLOAD) :
  {
    . = ALIGN(8);
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >DTCMRAM

  /* буфери за DMA (SPI към Linux платката, генериране на стъпки); без инициализация */
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    *(.dma_buffer) *(.dma_buffer*)
    . = ALIGN(32);
  } >RAM_D2

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
