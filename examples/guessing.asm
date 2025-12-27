; CASM Number Guessing Game
; Computer picks random 1-9, you guess!
; Demonstrates: rnd, inp, cmp, conditional branches

.text
_start:
    ; Generate random number 1-9
    mov x0, #9
    rnd
    add x10, x0, #1     ; x10 = secret (1-9)
    
    mov x11, #0         ; x11 = guess count

    ; Print "Guess 1-9:"
    mov w0, #71         ; 'G'
    prtc
    mov w0, #117        ; 'u'
    prtc
    mov w0, #101        ; 'e'
    prtc
    mov w0, #115        ; 's'
    prtc
    mov w0, #115        ; 's'
    prtc
    mov w0, #32         ; ' '
    prtc
    mov w0, #49         ; '1'
    prtc
    mov w0, #45         ; '-'
    prtc
    mov w0, #57         ; '9'
    prtc
    mov w0, #10         ; newline
    prtc

game_loop:
    ; Increment guess count
    add x11, x11, #1

    ; Print prompt "> "
    mov w0, #62         ; '>'
    prtc
    mov w0, #32         ; ' '
    prtc

    ; Read single digit
    inp
    mov x12, x0         ; save char
    
    ; Echo the character
    mov w0, w12
    prtc
    
    ; Read and discard newline
    inp
    
    ; Print newline
    mov w0, #10
    prtc
    
    ; Convert ASCII to number (subtract '0')
    sub x12, x12, #48

    ; Compare guess (x12) with secret (x10)
    cmp x12, x10
    b.eq win
    b.lt too_low

too_high:
    ; Print "Too high!"
    mov w0, #84         ; 'T'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #32         ; ' '
    prtc
    mov w0, #104        ; 'h'
    prtc
    mov w0, #105        ; 'i'
    prtc
    mov w0, #103        ; 'g'
    prtc
    mov w0, #104        ; 'h'
    prtc
    mov w0, #33         ; '!'
    prtc
    mov w0, #10
    prtc
    b game_loop

too_low:
    ; Print "Too low!"
    mov w0, #84         ; 'T'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #32         ; ' '
    prtc
    mov w0, #108        ; 'l'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #119        ; 'w'
    prtc
    mov w0, #33         ; '!'
    prtc
    mov w0, #10
    prtc
    b game_loop

win:
    ; Print "Correct! Tries: "
    mov w0, #67         ; 'C'
    prtc
    mov w0, #111        ; 'o'
    prtc
    mov w0, #114        ; 'r'
    prtc
    mov w0, #114        ; 'r'
    prtc
    mov w0, #101        ; 'e'
    prtc
    mov w0, #99         ; 'c'
    prtc
    mov w0, #116        ; 't'
    prtc
    mov w0, #33         ; '!'
    prtc
    mov w0, #32         ; ' '
    prtc
    
    ; Print guess count
    mov x0, x11
    prtn
    
    ; Print " tries"
    mov w0, #32         ; ' '
    prtc
    mov w0, #116        ; 't'
    prtc
    mov w0, #114        ; 'r'
    prtc
    mov w0, #105        ; 'i'
    prtc
    mov w0, #101        ; 'e'
    prtc
    mov w0, #115        ; 's'
    prtc
    mov w0, #10
    prtc

    halt
