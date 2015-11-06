A DNP3 parser implementation in Hammer 
Sven M. Hallberg, 2014-2015

* Uses the [Hammer](https://github.com/UpstandingHackers/hammer) parser combinator library.   
* Currently requires a (custom version)[https://github.com/pesco/hammer] (master branch) of Hammer, including:   
     * 'h_aligned', to check for byte-alignment (from branch 'h_aligned')
     * A temporary patch to 'h_put_value' allowing it to overwrite a
       previously-stored value. This is a stop-gap workaround until we get
       symbol scoping.

* Uses [cmake](https://cmake.org) to build. The 'build' directory is in .gitignore.

```sh
> mkdir build; cd build
> cmake ..
> make
```

* Run './dnp3-tests' to execute the unit tests (uses the GLib test framework).

* The './pprint' utility is an example application that accepts DNP3 traffic
   (as a stream of raw link-layer frames) on stdin and prints it in human-
   readable form. The 'samples/' directory contains some sample inputs in
   hexadecimal notation. Example usage:

```
xxd -r -p ../samples/read.hex | ./pprint
```

* The './filter' program uses the same traffic recognizer to sanitize its input. Try:

```
cat ../samples/*.hex | xxd -r -p | ./filter | ./pprint
```


NOTES:

 Z) A couple of bigger and a lot of smaller things are still TODO.

 A) This implementation is based on IEEE Std. 1815-2012 and notably
    AN2013-004b.

 B) This is an application-oriented implementation of DNP3. Nonconforming
    input is discarded immediately and not processed. The focus is on
    recognizing expected input as opposed to inspecting erroneous input.
   
    In the same vein, semantic representation is relatively high-level and does
    not insist on mirroring packet structure where details are irrelevant to a
    user of the protocol.

 C) Reserved bits are _required_ to be 0 unless the standard explicitly
    allows them to be ignored.

 D) Deprecated and obsolete parts of the protocol are not supported unless
    required. Specifically:

     - The INITIALIZE_DATA function code is rejected with a FUNC_NOT_SUPP
       parse error.
     - The SAVE_CONFIG function code is rejected with FUNC_NOT_SUPP.
     - The 'rollover' flag on counter objects is ignored and its value not
       exposed to the user.
     - Delta counters are rejected with an OBJ_UNKNOWN parse error.

     - The 'queue' flag on CROB objects _is_ accepted so an implementation
       can reject only the offending object in a request containing multiple
       commands.


LICENSE:

 TBD
