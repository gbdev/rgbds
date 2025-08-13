section "test", rom0
db 1, 4, 9, 16

section "entrypoint", rom0[$100]
ld b, b
jp Start

section "start", rom0[$150]
Start::
jp Start
