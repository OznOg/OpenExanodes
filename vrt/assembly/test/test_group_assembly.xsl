<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0"
	        xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:svg="http://www.w3.org/2000/svg">

  <xsl:variable name="x_offset">10</xsl:variable>
  <xsl:variable name="y_offset">10</xsl:variable>
  <xsl:variable name="chunk_width">100</xsl:variable>
  <xsl:variable name="chunk_height">40</xsl:variable>
  <xsl:variable name="chunk_margin">5</xsl:variable>
  <xsl:variable name="slot_height">
    <xsl:value-of select="$chunk_height + 2 * $chunk_margin"/>
  </xsl:variable>
  <xsl:variable name="slot_margin">5</xsl:variable>
  <xsl:variable name="text_offset">20</xsl:variable>

  <xsl:template match="assembly">
    <xsl:variable name="x_size">
      <xsl:value-of select="count(./slot[1]/chunk) * ($chunk_width + 2 * $chunk_margin)
                            + 2 * $x_offset"/>
    </xsl:variable>
    <xsl:variable name="y_size">
      <xsl:value-of select="$y_offset + count(./slot) * ($slot_height + $slot_margin)"/>
    </xsl:variable>
    <svg:svg version="1.1" xmlns="http://www.w3.org/2000/svg">
      <xsl:attribute name="width"><xsl:value-of select="$x_size"/></xsl:attribute>
      <xsl:attribute name="height"><xsl:value-of select="$y_size"/></xsl:attribute>
      <style type="text/css">
        .disk0  {fill: #6BCAE2}
        .disk1  {fill: #51A5BA}
        .disk2  {fill: #41924B}
        .disk3  {fill: #AFEAAA}
        .disk4  {fill: #87E293}
        .disk5  {fill: #FE84E2}
        .disk6  {fill: #7AF5F5}
        .disk7  {fill: #C6CFD6}
        .disk8  {fill: #B76EB8}
        .disk9  {fill: #FFFF66}
        .disk10 {fill: #FF9900}
        .disk11 {fill: #CC9900}
      </style>
      <xsl:apply-templates select="slot"/>
    </svg:svg>
  </xsl:template>

  <xsl:template match="slot">
    <xsl:variable name="y">
      <xsl:value-of select="(position() - 1) * ($slot_height + $slot_margin) + $y_offset"/>
    </xsl:variable>
    <xsl:variable name="width">
      <xsl:value-of select="count(./chunk) * ($chunk_width + 2 * $chunk_margin)"/>
    </xsl:variable>
    <svg:rect fill="none" stroke="black" stroke-width="2">
      <xsl:attribute name="x"><xsl:value-of select="$x_offset"/></xsl:attribute>
      <xsl:attribute name="y"><xsl:value-of select="$y"/></xsl:attribute>
      <xsl:attribute name="width"><xsl:value-of select="$width"/></xsl:attribute>
      <xsl:attribute name="height"><xsl:value-of select="$slot_height"/></xsl:attribute>
    </svg:rect>
    <xsl:apply-templates select="chunk">
      <xsl:with-param name="y"><xsl:value-of select="$y + $chunk_margin"/></xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="chunk">
    <xsl:param name="y">0</xsl:param>
    <xsl:variable name="x">
      <xsl:value-of select="(position() - 1) * ($chunk_width + 2 * $chunk_margin) +
                            $x_offset + $chunk_margin"/>
    </xsl:variable>
    <svg:rect stroke="black" stroke-width="2">
      <xsl:attribute name="width"><xsl:value-of select="$chunk_width"/></xsl:attribute>
      <xsl:attribute name="height"><xsl:value-of select="$chunk_height"/></xsl:attribute>
      <xsl:attribute name="x"><xsl:value-of select="$x"/></xsl:attribute>
      <xsl:attribute name="y"><xsl:value-of select="$y"/></xsl:attribute>
      <xsl:attribute name="class"><xsl:value-of select="concat('disk', @rdev_index)"/></xsl:attribute>
    </svg:rect>
    <svg:text>
      <!-- Attributes before value! -->
      <xsl:attribute name="x"><xsl:value-of select="$x + $text_offset"/></xsl:attribute>
      <xsl:attribute name="y"><xsl:value-of select="$y + $text_offset"/></xsl:attribute>
      <xsl:value-of select="concat(@rdev_index, ' - ', @chunk_id)"/>
    </svg:text>
  </xsl:template>

</xsl:stylesheet>
