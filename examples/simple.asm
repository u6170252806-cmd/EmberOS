; Simple CMP and branch test
; Should print: 123Done!

.text
_start:
    mov x10, #1         ; counter = 1
    mov x11, #4         ; limit = 4

loop:
    ; Print counter
    mov x0, x10
    prtn

    ; Increment
    add x10, x10, #1

    ; Compare and branch
    cmp x10, x11
    b.lt loop           ; if counter < 4, loop

    ; Print "Done!"
    mov w0, #68
    prtc
    mov w0, #111
    prtc
    mov w0, #110
    prtc
    mov w0, #101
    prtc
    mov w0, #33
    prtc
    mov w0, #10
    prtc

    halt
