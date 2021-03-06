Quickstart
==========

This document will go through building and running |project|.

Building Sigil2
---------------

.. note:: The default compiler for **CentOS 7** and older (gcc <5)
          does not support **C++14**. Install and enable the offical
          Devtoolset_ before compiling.

.. _Devtoolset:
   https://www.softwarecollections.org/en/scls/rhscl/devtoolset-6/

Clone and build |project| from source::

  $ git clone https://github.com/VANDAL/sigil2
  $ cd sigil2
  $ mkdir build && cd build
  $ cmake{3} .. # CentOS 7 requires cmake3 package
  $ make -j

This creates a ``build/bin`` folder containing the :program:`sigil2` executable.
It can be run in place, or the entire ``bin`` folder can be moved,
although it's not advised to move it to a system location.

Running Sigil2
--------------

|project| requires at least two arguments: the ``backend`` analysis tool,
and the ``executable`` application to measure::

  $ bin/sigil2 --backend=stgen --executable=./mybinary

The ``backend`` is the analysis tool that will process all the events
in ``mybinary``. In this example, ``stgen`` is the backend that processes
events into a special event trace that is used in SynchroTrace_.

More information on backends are in :ref:`backends`.

.. _SynchroTrace:
   http://vlsi.ece.drexel.edu/index.php/SynchroTrace/

A third option ``frontend`` will change the underlying method
for observing the application. By default, this is Valgrind_: ::

  $ bin/sigil2 --frontend=valgrind --backend=stgen --executable=./mybinary

.. _Valgrind: http://valgrind.org/

Available frontends are discussed in :ref:`frontends`.

Dependencies
------------

+-------------+----------+
| PACKAGE     | VERSION  |
+=============+==========+
| gcc/g++     |  5+      |
+-------------+----------+
| cmake       |  3.1.3+  |
+-------------+----------+
| make        |  3.8+    |
+-------------+----------+
| automake    |  1.13+   |
+-------------+----------+
| autoconf    |  2.69+   |
+-------------+----------+
| zlib        |  1.27+   |
+-------------+----------+
| git         |  1.8+    |
+-------------+----------+
