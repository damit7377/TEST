/*
******************************************************************************
**
** @file        : LinkerScript.ld
**
** @author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H7Rxx Device
**                      128KBytes FLASH
**                      456KBytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
******************************************************************************
** @attention
**
** Copyright (c) 2026 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
******************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCM) + LENGTH(DTCM); /* end of "DTCM" Ram type memory */

_Min_Heap_Size = 0x200; /* required amount of heap */
_Min_Stack_Size = 0x400; /* required amount of stack */

__FLASH_BEGIN  = 0x70000000;
__FLASH_SIZE   = 0x02000000;
__APP_FLASH_SIZE = 0x00100000;
__TOUCHGFX_ASSETS_BEGIN = __FLASH_BEGIN + __APP_FLASH_SIZE;
__TOUCHGFX_ASSETS_SIZE = __FLASH_SIZE - __APP_FLASH_SIZE;
__EXTRAM_BEGIN = 0x90000000;
__EXTRAM_SIZE  = 0x01000000;
__LCD_FB_SIZE  = 1024 * 600 * 2 * 2;


__RAM_BEGIN    = 0x24000000;
__RAM_SIZE     = 0x71C00;
__RAM_NONCACHEABLEBUFFER_SIZE = 0x400;

/* Memories definition */
MEMORY
{
  RAM       (xrw) : ORIGIN = __RAM_BEGIN,    LENGTH = __RAM_SIZE
  RAM_NONCACHEABLEBUFFER (xrw) : ORIGIN = __RAM_BEGIN + __RAM_SIZE,  LENGTH = __RAM_NONCACHEABLEBUFFER_SIZE

  ITCM      (xrw) : ORIGIN = 0x00000000,    LENGTH = 0x00010000
  DTCM       (rw) : ORIGIN = 0x20000000,    LENGTH = 0x00010000
  SRAMAHB   (rw)  : ORIGIN = 0x30000000,  LENGTH = 0x00008000
  BKPSRAM   (rw)  : ORIGIN = 0x38800000,  LENGTH = 0x00001000

  FLASH     (xrw) : ORIGIN = __FLASH_BEGIN, LENGTH = __APP_FLASH_SIZE
  TOUCHGFX_ASSETS (xrw) : ORIGIN = __TOUCHGFX_ASSETS_BEGIN, LENGTH = __TOUCHGFX_ASSETS_SIZE
  LCD_FB    (rw)  : ORIGIN = __EXTRAM_BEGIN, LENGTH = __LCD_FB_SIZE
  EXTRAM    (rw)  : ORIGIN = __EXTRAM_BEGIN + __LCD_FB_SIZE, LENGTH = __EXTRAM_SIZE - __LCD_FB_SIZE
}

/* Sections */
SECTIONS
{
  .lcd_framebuffer (NOLOAD) :
  {
    __lcd_framebuffer_start__ = .;
    . = . + LENGTH(LCD_FB);
    __lcd_framebuffer_end__ = .;
  } >LCD_FB

  .nemagfx_buffers (NOLOAD) :
  {
    . = ALIGN(32);
    KEEP(*(Nemagfx_Memory_Pool_Buffer))
    . = ALIGN(32);
    KEEP(*(Nemagfx_Stencil_Buffer))
    . = ALIGN(32);
  } >EXTRAM

  /* The startup code into "FLASH" FLASH type memory */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data into "FLASH" FLASH type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data into "FLASH" FLASH type memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .touchgfx_assets :
  {
    . = ALIGN(4);
    KEEP(*(TextFlashSection))
    KEEP(*(TextFlashSection.*))
    KEEP(*(FontFlashSection))
    KEEP(*(FontFlashSection.*))
    KEEP(*(FontSearchFlashSection))
    KEEP(*(FontSearchFlashSection.*))
    KEEP(*(ExtFlashSection))
    KEEP(*(ExtFlashSection.*))
    . = ALIGN(4);
  } >TOUCHGFX_ASSETS

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH
  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */

  } >RAM AT> FLASH

  /* Uninitialized data section into "RAM" Ram type memory */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  RW_NONCACHEABLE (NOLOAD):
  {
    __NONCACHEABLEBUFFER_BEGIN = .;/* create symbol for start of section */
    KEEP(*(noncacheable_buffer))
    __NONCACHEABLEBUFFER_END = .;  /* create symbol for start of section */
  } > RAM_NONCACHEABLEBUFFER

  /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >DTCM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
