$Id$

CVS backend for Version Control API -
Provides CVS commit information and account management as a pluggable backend.


SHORT DESCRIPTION
-----------------
This module is a work in progress, and not yet ready to use.
It's one of those modules being created within the Google Summer of Code 2007.
When it's done, the description might look approximately like this:

This module provides an implementation of the Version Control API that makes
it possible to use the CVS version control system. It can retrieve commit
information by parsing commit logs or by having the xcvs-* scripts called by
into CVS's commit hooks, and is able to programmatically manage CVS accounts.

For the API documentation, have a look at the module file or run doxygen/phpdoc
on it to get a fancier version of the docs.


AUTHOR
------
Jakob Petsovits <jpetso at gmx DOT at>


CREDITS
-------
A lot if code in the CVS backend was taken from the CVS integration module
(cvs.module) on drupal.org, where the adapted sections were committed by:

Derek Wright ("dww", http://drupal.org/user/46549)
Karthik ("Zen", http://drupal.org/user/21209)

This module was originally created as part of Google Summer of Code 2007,
so Google also deserves some credits for making this possible.
