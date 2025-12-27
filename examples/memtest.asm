; CASM Memory Ops Test
; Tests: strlen, memcpy, memset, abs, prtx

.text
_start:
    ; Build "Hello" at 0x500
    mov x10, #0x500
    mov w0, #72
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #101
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #108
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #108
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #111
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #0
    strb w0, [x10]

    ; Test 1: strlen
    mov x0, #0x500
    strlen
    mov x8, x0
    mov w0, #49
    prtc
    mov w0, #46
    prtc
    mov w0, #108
    prtc
    mov w0, #101
    prtc
    mov w0, #110
    prtc
    mov w0, #61
    prtc
    mov x0, x8
    prtn
    mov w0, #10
    prtc

    ; Test 2: memcpy 0x500->0x520
    mov x0, #0x520
    mov x1, #0x500
    mov x2, #6
    memcpy
    mov w0, #50
    prtc
    mov w0, #46
    prtc
    mov w0, #99
    prtc
    mov w0, #112
    prtc
    mov w0, #121
    prtc
    mov w0, #61
    prtc
    mov x0, #0x520
    prt
    mov w0, #10
    prtc

    ; Test 3: memset 0x540 with *
    mov x0, #0x540
    mov x1, #42
    mov x2, #5
    memset
    mov x10, #0x545
    mov w0, #0
    strb w0, [x10]
    mov w0, #51
    prtc
    mov w0, #46
    prtc
    mov w0, #115
    prtc
    mov w0, #101
    prtc
    mov w0, #116
    prtc
    mov w0, #61
    prtc
    mov x0, #0x540
    prt
    mov w0, #10
    prtc

    ; Test 4: abs(42)
    mov x0, #42
    abs
    mov x8, x0
    mov w0, #52
    prtc
    mov w0, #46
    prtc
    mov w0, #97
    prtc
    mov w0, #98
    prtc
    mov w0, #115
    prtc
    mov w0, #61
    prtc
    mov x0, x8
    prtn
    mov w0, #10
    prtc

    ; Test 5: abs(-25)
    mov x0, #0
    sub x0, x0, #25
    abs
    mov x8, x0
    mov w0, #53
    prtc
    mov w0, #46
    prtc
    mov w0, #97
    prtc
    mov w0, #98
    prtc
    mov w0, #115
    prtc
    mov w0, #61
    prtc
    mov x0, x8
    prtn
    mov w0, #10
    prtc

    ; Test 6: prtx(255)
    mov w0, #54
    prtc
    mov w0, #46
    prtc
    mov w0, #104
    prtc
    mov w0, #101
    prtc
    mov w0, #120
    prtc
    mov w0, #61
    prtc
    mov x0, #255
    prtx
    mov w0, #10
    prtc

    ; Done - print OK
    mov w0, #79
    prtc
    mov w0, #75
    prtc
    mov w0, #10
    prtc
    halt
