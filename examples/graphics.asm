; CASM Graphics Demo
; Uses virtual framebuffer - rendered after execution
; Opcodes: canvas, setc, box, plot, reset, rnd

.text
_start:
    ; Set up canvas (width=30, height=8)
    ; Graphics are buffered and rendered at the end
    mov w0, #30         ; width
    mov w1, #8          ; height
    canvas

    ; Set colors: white on blue
    mov w0, #7          ; white foreground
    mov w1, #4          ; blue background
    setc

    ; Draw a box at position 0,0 with size 28x7
    mov x0, #0          ; x position
    mov x1, #0          ; y position
    mov x2, #28         ; width
    mov x3, #7          ; height
    box

    ; Plot some stars inside the box
    mov x0, #5          ; x
    mov x1, #2          ; y
    mov w2, #42         ; '*'
    plot

    mov x0, #14         ; x
    mov x1, #4          ; y
    mov w2, #42         ; '*'
    plot

    mov x0, #22         ; x
    mov x1, #3          ; y
    mov w2, #42         ; '*'
    plot

    ; Reset colors for text output
    reset

    ; Print text (appears before the canvas render)
    mov w0, #82         ; 'R'
    prtc
    mov w0, #97         ; 'a'
    prtc
    mov w0, #110        ; 'n'
    prtc
    mov w0, #100        ; 'd'
    prtc
    mov w0, #58         ; ':'
    prtc
    mov w0, #32         ; ' '
    prtc

    ; Random number
    mov x0, #100
    rnd
    prtn
    mov w0, #10         ; newline
    prtc

    halt
