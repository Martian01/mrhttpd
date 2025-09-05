<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="html"/>
    <xsl:template match="/*">
        <html>
            <head>
                <title>Directory</title>
                <style type="text/css">
body { background: #F0F0F0; padding: 2cm; }
p { font-family: "Nimbus Sans L", "Helvetica", "Verdana", "Sans-serif"; }
h1 { font-family: "Nimbus Sans L", "Helvetica", "Verdana", "Sans-serif"; }
th, td { font-family: "Nimbus Sans L", "Helvetica", "Verdana", "Sans-serif"; text-align: left; padding: 1mm 5mm; }
tr:nth-child(even) { background-color: #E0E0E0; }
                </style>
            </head>
            <body>
                <h1>Directory Content</h1>
                <hr />
                <table>
                    <tr><th>Name</th><th>Type</th><th>Size</th><th>Modified</th></tr>
                    <xsl:apply-templates select="entry"/>
                </table>
                <hr />
                <i><xsl:apply-templates select="/directory/server"/> server time <xsl:apply-templates select="/directory/timestamp"/></i>
            </body>
        </html>
    </xsl:template>
    <xsl:template match="entry">
        <tr><xsl:apply-templates select="*"/></tr>
    </xsl:template>
    <xsl:template match="name">
        <td><a><xsl:attribute name="href"><xsl:apply-templates select="/directory/path"/><xsl:apply-templates select="text()"/></xsl:attribute><xsl:apply-templates select="text()"/></a></td>
    </xsl:template>
    <xsl:template match="type">
        <td><xsl:apply-templates select="text()"/></td>
    </xsl:template>
    <xsl:template match="size">
        <td><xsl:apply-templates select="text()"/></td>
    </xsl:template>
    <xsl:template match="modified">
        <td><xsl:apply-templates select="text()"/></td>
    </xsl:template>
</xsl:stylesheet>
