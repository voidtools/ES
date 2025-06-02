# ES
The Command Line Interface for Everything.

[Download](#download)<br/>
[Install Guide](#Install-Guide)<br/>
[Usage](#Usage)<br/>
[Search Options](#Search-Options)<br/>
<br/><br/><br/>



Download
--------

https://www.voidtools.com/downloads#cli
<br/><br/><br/>



![image](https://github.com/user-attachments/assets/0fcbe74a-c24c-4065-8a14-85757d525212)
<br/><br/><br/>



Install Guide
-------------

To install ES:
*   [Download](#download) ES and extract ES.exe to:<br>
    <code>%LOCALAPPDATA%\Microsoft\WindowsApps</code>
<br/><br/><br/>



Search Options
--------------

<dl>
<dt>-r &lt;search&gt;, -regex &lt;search&gt;</dt>
<dd>Search using regular expressions.</dd>
<dt>-i, -case</dt>
<dd>Match case.</dd>
<dt>-w, -ww, -whole-word, -whole-words</dt>
<dd>Match whole words.</dd>
<dt>-p, -match-path</dt>
<dd>Match full path and file name.</dd>
<dt>-a, -diacritics</dt>
<dd>Match diacritical marks.</dd>
<br/><br/>
<dt>-o &lt;offset&gt;, -offset &lt;offset&gt;</dt>
<dd>Show results starting from offset.</dd>
<dt>-n &lt;num&gt;, -max-results &lt;num&gt;</dt>
<dd>Limit the number of results shown to &lt;num&gt;.</dd>
<br/><br/>
<dt>-path &lt;path&gt;</dt>
<dd>Search for subfolders and files in path.</dd>
<dt>-parent-path &lt;path&gt;</dt>
<dd>Search for subfolders and files in the parent of path.</dd>
<dt>-parent &lt;path&gt;</dt></dd>
<dd>Search for files with the specified parent path.</dd>
</dl>
<br/><br/><br/>



Usage
-----

<code>es.exe [options] search text</code>

For example:

<code>es everything ext:exe;ini</code>
