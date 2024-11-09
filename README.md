# ZMK Sticky Modifiers

A ZMK module that adds a behavior called `sticky-modifiers`, heavily inspired by [this](https://github.com/rizo/qmk_firmware/blob/65085f69eab19b7eb13381c7ec14a616df8f6c2e/keyboards/ferris/keymaps/epsilon10/keymap.c#L254) ([reddit post that lead me to that brilliant idea](https://www.reddit.com/r/ErgoMechKeyboards/comments/w169sa/comment/liunyl3/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button)).

## What the behavior does
It acts mostly like the sticky key behavior, with some notable differences:
- **no timer involved**: the sticky key behavior has an expiration window. This behavior only acts upon user actions, making it usable as fast (or more importantly for me, as slow) as you want.
- **ability to trigger "double tap" or "tap then hold" actions**: one of the drivers that pushed me to implement this behavior is that all the Jetbrains IDE have some valuable shortcuts implemented as double taps (for example the extremely powerful [Search everywhere](https://blog.jetbrains.com/idea/2020/05/when-the-shift-hits-the-fan-search-everywhere/) binding is a double Shift tap, or the keyboard shortcut to create multiple cursors is Ctrl Ctrl(hold)). All these doubled actions will work with this behavior, which is not possible with the default sticky key behavior.
- **work as expected**: the default sticky key behavior has flaws (for example a momentary layer press will consume the sticky keys) that this behavior tries to avoid.

So the behavior decision tree is as follows:
- tap: the modifier is stored, and will be injected in the next non modifier key press
  - tap a key which is not a modifier: the sticky modifier is consumed and flushed.
  - tap other different sticky modifiers: the other modifiers are added to the stored combination. This allows to trigger complex shortcuts (like Ctrl+Alt+Shift).
  - tap or hold an already stored modifier: the tap/hold event of this modifier will be triggered (double tap, or tap and hold). Other stored modifiers will be forgotten.
- hold: the decision depends on the next action
  - hold other modifiers: same, the decision will be done as below
  - if a non modifier key is pressed while held: all sticky modifier keys will act as normal keys until the end of the interaction
  - else, the modifier key is stored as it would have been upon a tap