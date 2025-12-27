; CASM Auto-Canvas Graphics Test
; Draws a box without explicit canvas setup
; Tests auto-canvas feature (default 40x12)

.text
_start:
    ; Set colors: yellow on blue (no canvas needed - auto creates)
    mov w0, #3          ; yellow foreground
    mov w1, #4          ; blue background
    setc

    ; Draw a box at position 5,2 with size 20x6
    mov x0, #5          ; x
    mov x1, #2          ; y
    mov x2, #20         ; width
    mov x3, #6          ; height
    box

    ; Plot some stars inside
    mov x0, #10
    mov x1, #4
    mov w2, #42         ; '*'
    plot

    mov x0, #15
    mov x1, #4
    mov w2, #42         ; '*'
    plot

    mov x0, #20
    mov x1, #4
    mov w2, #42         ; '*'
    plot

    ; Reset colors
    reset

    ; Print message after graphics
    mov w0, #65         ; 'A'
    prtc
    mov w0, #117        ; 'u'
    prtc
    mov w0, #116        ; 't'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #33         ; '!'
    prtc
    mov w0, #10
    prtc

    halt
