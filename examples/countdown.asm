; CASM Countdown Timer Demo
; Counts down from 10 to 1 with timing display
; Demonstrates: tick, sleep, loops, arithmetic

.text
_start:
    ; Print header
    mov w0, #67         ; 'C'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #117        ; 'u'
    prtc
    mov w0, #110        ; 'n'
    prtc
    mov w0, #116        ; 't'
    prtc
    mov w0, #100        ; 'd'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #119        ; 'w'
    prtc
    mov w0, #110        ; 'n'
    prtc
    mov w0, #58         ; ':'
    prtc
    mov w0, #10         ; newline
    prtc

    ; Get start time
    tick
    mov x20, x0         ; save start time

    ; Counter = 10
    mov x10, #10

loop:
    ; Print current number
    mov x0, x10
    prtn
    mov w0, #46         ; '.'
    prtc
    mov w0, #46         ; '.'
    prtc
    mov w0, #46         ; '.'
    prtc
    mov w0, #10         ; newline
    prtc

    ; Sleep 500ms
    mov x0, #500
    sleep

    ; Decrement counter
    sub x10, x10, #1

    ; Check if counter > 0
    cmp x10, #0
    b.gt loop

    ; Print "DONE!"
    mov w0, #68         ; 'D'
    prtc
    mov w0, #79         ; 'O'
    prtc
    mov w0, #78         ; 'N'
    prtc
    mov w0, #69         ; 'E'
    prtc
    mov w0, #33         ; '!'
    prtc
    mov w0, #10
    prtc

    ; Get end time and calculate elapsed
    tick
    sub x0, x0, x20     ; elapsed = end - start
    
    ; Print elapsed time
    mov w0, #84         ; 'T'
    prtc
    mov w0, #105        ; 'i'
    prtc
    mov w0, #109        ; 'm'
    prtc
    mov w0, #101        ; 'e'
    prtc
    mov w0, #58         ; ':'
    prtc
    mov w0, #32         ; ' '
    prtc
    
    ; x0 still has elapsed time
    tick
    sub x0, x0, x20
    prtn
    
    mov w0, #109        ; 'm'
    prtc
    mov w0, #115        ; 's'
    prtc
    mov w0, #10
    prtc

    halt
