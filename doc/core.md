# backward compatibility

vkdt does not guarantee full backward compatibility of all modules, but
instead distinguishes *core* and *beta* modules.

modules with the *core* tag guarantee backward compatibility. we may still
change the settings/add new ones, but will provide near identical automatic
update paths. things like random hashes or bugfixes might still be
allowable changes.

*beta* modules are still in flux and their parameters might change. we try
to be reasonable and not introduce breaking changes unless really necessary.
use your judgement: if modules have very recently been introduced it's more
likely that the parameters need to settle a bit.
