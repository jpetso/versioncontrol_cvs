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
A good amount of code in Version Control / Project Node Integration was taken
from the CVS integration module on drupal.org, its authors deserve a lot of
credits and may also hold copyright for parts of this module.

This module was originally created as part of Google Summer of Code 2007,
so Google deserves some credits for making this possible. Thanks also
to Derek Wright (dww) and Andy Kirkham (AjK) for mentoring
the Summer of Code project.
