ENTRY(_start)
MEMORY
{
    rom(rwx) : ORIGIN = 0xf000000, LENGTH = 0x10000
}
SECTIONS
{
    .text :
    {
    } > rom
}
