; CASM Loop Demo
; Prints "12345" three times using nested loops
; Demonstrates: cmp, b.ge, b (unconditional), loops

.text
_start:
    ; Outer loop counter (3 iterations)
    mov x20, #0         ; outer = 0
    mov x21, #3         ; outer_limit = 3

outer_loop:
    ; Check if outer >= 3
    cmp x20, x21
    b.ge done           ; if outer >= 3, exit

    ; Inner loop: print 1 2 3 4 5
    mov x10, #1         ; inner = 1
    mov x11, #6         ; inner_limit = 6

inner_loop:
    ; Check if inner >= 6
    cmp x10, x11
    b.ge inner_done     ; if inner >= 6, exit inner loop

    ; Print the number
    mov x0, x10
    prtn

    ; Increment inner
    add x10, x10, #1
    b inner_loop

inner_done:
    ; Print newline after each row
    mov w0, #10
    prtc

    ; Sleep 500ms between rows
    mov x0, #500
    sleep

    ; Increment outer
    add x20, x20, #1
    b outer_loop

done:
    ; Print "Done!"
    mov w0, #68         ; 'D'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #110        ; 'n'
    prtc
    mov w0, #101        ; 'e'
    prtc
    mov w0, #33         ; '!'
    prtc
    mov w0, #10
    prtc

    halt
