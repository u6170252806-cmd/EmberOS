; EmberOS CASM Example - Hello World with Math
; Uses prtc to print characters and prtn for numbers
;
; Usage:
;   casm hello.asm -o hello.bin
;   casm run hello.bin

.text
_start:
    ; Print "Hello!" using prtc (character by character)
    mov w0, #72             ; 'H'
    prtc
    mov w0, #101            ; 'e'
    prtc
    mov w0, #108            ; 'l'
    prtc
    mov w0, #108            ; 'l'
    prtc
    mov w0, #111            ; 'o'
    prtc
    mov w0, #33             ; '!'
    prtc
    mov w0, #10             ; newline
    prtc

    ; Print "10 + 30 = "
    mov w0, #49             ; '1'
    prtc
    mov w0, #48             ; '0'
    prtc
    mov w0, #32             ; ' '
    prtc
    mov w0, #43             ; '+'
    prtc
    mov w0, #32             ; ' '
    prtc
    mov w0, #51             ; '3'
    prtc
    mov w0, #48             ; '0'
    prtc
    mov w0, #32             ; ' '
    prtc
    mov w0, #61             ; '='
    prtc
    mov w0, #32             ; ' '
    prtc

    ; Calculate 10 + 30 and print result
    mov x1, #10
    mov x2, #30
    add x0, x1, x2          ; x0 = 40
    prtn                    ; Print "40"

    mov w0, #10             ; newline
    prtc

    ; Print "5 * 8 = " then calculate using repeated addition
    mov w0, #53             ; '5'
    prtc
    mov w0, #32             ; ' '
    prtc
    mov w0, #42             ; '*'
    prtc
    mov w0, #32             ; ' '
    prtc
    mov w0, #56             ; '8'
    prtc
    mov w0, #32             ; ' '
    prtc
    mov w0, #61             ; '='
    prtc
    mov w0, #32             ; ' '
    prtc

    ; 5 * 8 = 8 + 8 + 8 + 8 + 8
    mov x0, #0
    mov x1, #8
    add x0, x0, x1
    add x0, x0, x1
    add x0, x0, x1
    add x0, x0, x1
    add x0, x0, x1
    prtn                    ; Print "40"

    mov w0, #10             ; newline
    prtc

    ; Print "Bye!"
    mov w0, #66             ; 'B'
    prtc
    mov w0, #121            ; 'y'
    prtc
    mov w0, #101            ; 'e'
    prtc
    mov w0, #33             ; '!'
    prtc
    mov w0, #10             ; newline
    prtc

    halt
