{$IFNDEF NO_FBCLIENT}
	function fb_get_master_interface : IMaster; cdecl; external 'fbclient';
{$ENDIF}

const
	FB_UsedInYValve = FALSE;
