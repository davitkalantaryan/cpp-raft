
CRaft
=====

What?
-----
C implementation of the Raft Consensus protocol, BSD licensed

How does it work?
-----------------
See raft.h for full documentation.

Networking is out of scope for this project. The implementor will need to do all the plumbing. *Currently*, this is done by:

- Implementing all the callbacks within raft_cbs_t; and
- Calling raft_recv_.* functions with msg_.* message structs

Dependencies
------------http://marketplace.eclipse.org/marketplace-client-intro?mpc_install=369
There are no dependencies, however https://github.com/willemt/CLinkedListQueue is required for testing.

Building
--------
$make

Todo
----
- Member changes
- Log compaction
- More scenario tests (ie. more varied network partition scenarios)
- Usage example


Reasons to fork
----
- Get rid of BOOST dependency
- Compile on Windows with Microsoft compiler


