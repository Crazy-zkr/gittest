#include "wsd_file.h"
#include "stdio.h"

BOOL ReadCfg(char *pPath, char *pTop, char *pBuf, BYTE byNum)
{
	FILE *fp;
	char *pos;
	char line[LINE_SIZE];
	WORD wLen;
	BOOL IsTop = FALSE;
	int offset = 1,i=0;

	fp = fopen(pPath, "r");
	if (NULLP == fp)
	{
		Trace("Open %s error", pPath);
		return FALSE;
	}
	offset = 1;
	i = 0;
	while (!feof(fp))
	{
		memset(line, 0, LINE_SIZE);
		fgets(line, LINE_SIZE, fp);
		if (line[0] == '#')
			continue;
		if (strstr(line, pTop))
		{
			IsTop = TRUE;
			continue;
		}
		if (!IsTop)
			continue;
		if (strstr(line, "[") && strstr(line, "]"))
			break;
		pos = strchr(line,'=');
		if (NULLP == pos)
			continue;
		i++;
		if (i!=byNum)
			continue;
		wLen = strlen(line);
		if (line[wLen - 1] == '\n')
			offset = 2;
		memcpy(pBuf, pos+1,wLen-offset);
		fclose(fp);
		return TRUE;
	}
	fclose(fp);
	return FALSE;
}