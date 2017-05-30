An extension of brainfuck that provides support for drawing to the screen and getting user input.

This interpreter is an extension of the one [here](https://github.com/brandonpickering/brainfuck), so it follows the same general brainfuck rules.

It extends the language with a 256x256 2D buffer of bytes and a cursor for this buffer, along with the following instructions:
- l, u, r, and d move the cursor, wrapping at the edges of the buffer
- ' writes the value at the data pointer to the buffer at the cursor's position
- : displays the buffer to the screen and limits the framerate to 60fps
- ; places the key value of a key that the user is currently pressing into the cell at the data pointer. When called repeatedly, it will list all the keys being held down, or 0 when it is complete. This resets after every : call.

The key values are taken from GLFW for key values less than 256. For values greater than 255, we subtract 128 from the GLFW values.
