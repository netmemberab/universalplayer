







! OUT OF DATE !


no TURBO-C port.

from previous implementation: 

TURBO-C mixdev.cc:

  In TurboC++ Player will currently not allow Samples bigger than 64KB,
  that's because TC allows data>64KB in HUGE-module but does not generate
  automaticly HUGE-pointers which can handle segment overides. I tried
  to catch this with   smp_ptr as "char huge *" ... but TCs generated 
  hugeptr-arithmetic fucks up ... I'M GOING MAD WITH THIS STUPID DOS 
  SEGMENTATION !!!  HEY MAN, SWITCH TO DJGPP !!

----> might be solved with new mixing routines, but there're still some minor
      bugs - TurboC port is anyway not yet done.

SOLARIS:

  Player output is wrong - I don't know yet how to encode sound data correct.
  (8bit to linear what is it like ?)

SUN:

  No output - I have to implement data encoding function 8bit to ulaw (in mixdev) 
  - i know the algorithm, but I've no permission to use the sun audio device on
  my univercitys SUN stations and test the results.

XM.cc:

  Autovibrato is overwritten if effects do pitch changes...

  Finetuning with amiga frequencies is not fully (!) implemented and might sound
  wrong.

  THE   "M1 ---"
        "M0 301"    BUG. why da hell FT2 uses Porta 0x10 here ???? and not 0x01 ??
			.... or does it 0x11 ?

  panning enevelope is not implemented yet.

S3M.cc:

  Does not work with S3Ms with empty pattern list. (was my lazyness)
  
  Arpeggio not correct if portamento was before.

  Effects "set finetune" & "multi retrigg" not yet implemented. (lazyness,
  the multiretrigg is equal to XM-multiretrigger)

MOD.cc:

  Arpeggio correct ???
  FLT8 is wrong - any example MOD in this format ? I know about it, but I want
  something for testing.

669.cc:

  Can anybody tell me about the correct tempos ?

ALL devices:
  - jumps above sample loops -> sample is nonlooping after that
    (play it to its sampleend)

AWE support:

  - ping-pong-loops not yet (unroll them).
  - Set-Panning is buggy - wired output ..
  - Set-Sample-Offset at sample borders....might cause wired output.
  - 'channel free'-checking is not optimal

MixDevice:

  - "negativ dir & change frequency"-BUG
