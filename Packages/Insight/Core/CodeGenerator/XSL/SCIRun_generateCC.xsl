<?xml version="1.0"?> 
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:output method="text" indent="yes"/>



<!-- ============= GLOBALS =============== -->
<xsl:variable name="root-name">
  <xsl:value-of select="/filter/@name"/>
</xsl:variable>
<xsl:variable name="sci-name">
  <xsl:value-of select="/filter/filter-sci/@name"/>
</xsl:variable>
<xsl:variable name="itk-name">
  <xsl:value-of select="/filter/filter-itk/@name"/>
</xsl:variable>
<xsl:variable name="package">
  <xsl:value-of select="/filter/filter-sci/package"/>
</xsl:variable>
<xsl:variable name="category">
  <xsl:value-of select="/filter/filter-sci/category"/>
</xsl:variable>



<!-- ================ FILTER ============= -->
<xsl:template match="/filter">
<xsl:call-template name="header" />
<xsl:call-template name="includes" />
namespace <xsl:value-of select="$package"/> 
{

using namespace SCIRun;

<xsl:call-template name="create_class_decl" />

<xsl:call-template name="define_run" />

DECLARE_MAKER(<xsl:value-of select="/filter/filter-sci/@name"/>)

<xsl:call-template name="define_constructor" />
<xsl:call-template name="define_destructor" />
<xsl:call-template name="define_execute" />
<xsl:call-template name="define_tcl_command" />
} // End of namespace Insight
</xsl:template>



<!-- ============= HELPER FUNCTIONS ============ -->

<!-- ====== HEADER ====== -->
<xsl:template name="header">
<xsl:call-template name="output_copyright" />
<xsl:text>/*
 * </xsl:text><xsl:value-of select="$sci-name"/><xsl:text>.cc
 *
 *   Auto Generated File For </xsl:text><xsl:value-of select="$itk-name"/><xsl:text>
 *
 */

</xsl:text>
</xsl:template>


<!-- ======= OUTPUT_COPYRIGHT ====== -->
<xsl:template name="output_copyright">
<xsl:text>/*
  The contents of this file are subject to the University of Utah Public
  License (the &quot;License&quot;); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an &quot;AS IS&quot;
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/

</xsl:text>
</xsl:template>

<!-- ====== INCLUDES ====== -->
<xsl:template name="includes">
#include &lt;Dataflow/Network/Module.h&gt;
#include &lt;Core/Malloc/Allocator.h&gt;
#include &lt;Core/GuiInterface/GuiVar.h&gt;
#include &lt;Packages/Insight/share/share.h&gt;
<xsl:for-each select="/filter/filter-sci/includes/file">
#include &lt;<xsl:value-of select="."/>&gt;
</xsl:for-each>
<xsl:for-each select="/filter/filter-itk/includes/file">
#include &lt;<xsl:value-of select="."/>&gt;
</xsl:for-each>
</xsl:template>



<!-- ====== CREATE_CLASS_DECL ====== -->
<xsl:template name="create_class_decl">
<xsl:text>class </xsl:text><xsl:value-of select="$package"/><xsl:text>SHARE </xsl:text>
<xsl:value-of select="/filter/filter-sci/@name"/>
<xsl:text> : public Module 
{
public:

  // Declare GuiVars
  </xsl:text>
<xsl:call-template name="declare_guivars"/>
<xsl:text>  
  // Declare Ports
  </xsl:text>
<xsl:call-template name="declare_ports_and_handles" />
<xsl:text>
  </xsl:text>
<xsl:value-of select="$sci-name"/>
<xsl:text>(GuiContext*);

  virtual ~</xsl:text><xsl:value-of select="$sci-name"/>
<xsl:text disable-output-escaping="yes">();

  virtual void execute();

  virtual void tcl_command(GuiArgs&amp;, void*);

  // Run function will dynamically cast data to determine which
  // instantiation we are working with. The last template type
  // refers to the last template type of the filter intstantiation.
  template&lt;</xsl:text>
<xsl:for-each select="/filter/filter-itk/templated/template">
<xsl:variable name="type"><xsl:value-of select="@type"/></xsl:variable>
<xsl:choose>
   <xsl:when test="$type = ''">class </xsl:when>
   <xsl:otherwise><xsl:value-of select="$type"/><xsl:text> </xsl:text> </xsl:otherwise>
</xsl:choose> 
<xsl:value-of select="."/>
<xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if>
</xsl:for-each><xsl:text> &gt; 
  bool run( </xsl:text>
<xsl:for-each select="/filter/filter-itk/inputs/input">
<xsl:variable name="import"><xsl:value-of select="@import"/></xsl:variable>
<xsl:choose>
  <xsl:when test="$import = 'yes'">
  <xsl:variable name="import_name"><xsl:value-of select="value"/></xsl:variable>itk::Object*</xsl:when>
  <xsl:otherwise>itk::Object* </xsl:otherwise>
</xsl:choose>
<xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if></xsl:for-each> );
};

</xsl:template>



<!-- ====== DECLARE_GUIVARS ====== -->
<xsl:template name="declare_guivars">
<xsl:for-each select="/filter[@name=$root-name]/filter-itk[@name=$itk-name]/parameters/param">
<xsl:variable name="type"><xsl:value-of select="type"/></xsl:variable>
<xsl:choose>
<xsl:when test="$type = 'double'">GuiDouble </xsl:when>
<xsl:when test="$type = 'float'">GuiDouble </xsl:when>
<xsl:when test="$type = 'int'">GuiInt </xsl:when>
<xsl:when test="$type = 'bool'">GuiInt </xsl:when>
<xsl:otherwise>
GuiDouble </xsl:otherwise>
</xsl:choose>
<xsl:text> gui_</xsl:text><xsl:value-of select="name"/>
<xsl:text>_;
  </xsl:text>
</xsl:for-each>
</xsl:template>




<!-- ====== DECLARE_PORTS_AND_HANDLES ====== -->
<xsl:template name="declare_ports_and_handles">

<xsl:for-each select="/filter/filter-itk/inputs/input">
<xsl:variable name="type-name"><xsl:value-of select="value"/></xsl:variable>
<xsl:variable name="iport">inport<xsl:value-of select="@num"/></xsl:variable>
<xsl:variable name="ihandle">inhandle<xsl:value-of select="@num"/></xsl:variable>
<xsl:variable name="import"><xsl:value-of select="@import"/></xsl:variable>
<xsl:variable name="optional"><xsl:value-of select="@optional"/></xsl:variable>
<xsl:choose>
  <xsl:when test="$import = 'yes'">
  <xsl:variable name="import_filter"><xsl:value-of select="value"/></xsl:variable>
  <xsl:variable name="import_port"><xsl:value-of select="port"/></xsl:variable>
  <xsl:variable name="import_type"><xsl:value-of select="/filter/filter-itk/filter[@name=$import_filter]/filter-itk/inputs/input[@num=$import_port]/value"/></xsl:variable><!-- hard coded datatype --><xsl:text>  </xsl:text>ITKDatatype<xsl:text>IPort* </xsl:text><xsl:value-of select="$iport"/><xsl:text>_;
  </xsl:text><!-- hard coded datatype --><xsl:text>ITKDatatypeHandle </xsl:text><xsl:value-of select="$ihandle"/>_;
  </xsl:when>
  <xsl:otherwise>
<!-- hard coded datatype --><xsl:text>ITKDatatypeIPort* </xsl:text><xsl:value-of select="$iport"/><xsl:text>_;
  </xsl:text><!-- hard coded datatype --><xsl:text>ITKDatatypeHandle </xsl:text><xsl:value-of select="$ihandle"/>_;
  </xsl:otherwise>
</xsl:choose>

<xsl:if test="$optional = 'yes'">  bool <xsl:value-of select="$iport"/>_has_data_;
</xsl:if>
<xsl:text>
  </xsl:text>
</xsl:for-each>

<xsl:for-each select="/filter/filter-itk/outputs/output">
<xsl:variable name="type-name"><xsl:value-of select="value"/></xsl:variable>
<xsl:variable name="oport">outport<xsl:value-of select="@num"/></xsl:variable>
<xsl:variable name="ohandle">outhandle<xsl:value-of select="@num"/></xsl:variable>
<xsl:variable name="import"><xsl:value-of select="@import"/></xsl:variable>
<xsl:choose>
  <xsl:when test="$import = 'yes'">
  <xsl:variable name="import_filter"><xsl:value-of select="value"/></xsl:variable>
  <xsl:variable name="import_port"><xsl:value-of select="port"/></xsl:variable>
  <xsl:variable name="import_type"><xsl:value-of select="/filter/filter-itk/filter[@name=$import_filter]/filter-itk/outputs/output[@num=$import_port]/value"/></xsl:variable>
<!-- hard coded datatype --><xsl:text>ITKDatatypeIPort* </xsl:text><xsl:value-of select="$oport"/><xsl:text>_;
  </xsl:text><!-- hard coded datatype --><xsl:text>ITKDatatypeHandle </xsl:text><xsl:value-of select="$ohandle"/>_;
  </xsl:when>
  <xsl:otherwise>
<!-- hard coded datatype --><xsl:text>ITKDatatypeOPort* </xsl:text><xsl:value-of select="$oport"/><xsl:text>_;
  </xsl:text><!-- hard coded datatype --><xsl:text>ITKDatatypeHandle </xsl:text><xsl:value-of select="$ohandle"/><xsl:text>_;

  </xsl:text>
  </xsl:otherwise>
</xsl:choose>
<!-- hard coded datatype --><xsl:text>ITKDatatype* out</xsl:text><xsl:value-of select="@num"/>_;<xsl:text>
  </xsl:text>
</xsl:for-each>
</xsl:template>



<!-- ====== DEFINE_RUN ====== -->
<xsl:template name="define_run">
<xsl:text disable-output-escaping="yes">
template&lt;</xsl:text>
<xsl:for-each select="/filter/filter-itk/templated/template">
<xsl:variable name="type"><xsl:value-of select="@type"/></xsl:variable>
<xsl:choose>
   <xsl:when test="$type = ''">class </xsl:when>
   <xsl:otherwise><xsl:value-of select="$type"/><xsl:text> </xsl:text> </xsl:otherwise>
</xsl:choose>

<xsl:value-of select="."/>
<xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if>
</xsl:for-each><xsl:text>&gt;
bool </xsl:text><xsl:value-of select="$sci-name"/><xsl:text>::run( </xsl:text>

<xsl:for-each select="/filter/filter-itk/inputs/input">
<xsl:variable name="var">obj<xsl:value-of select="@num"/> </xsl:variable>
<xsl:variable name="import"><xsl:value-of select="@import"/></xsl:variable>
<xsl:choose>
  <xsl:when test="$import = 'yes'">
  <xsl:variable name="import_name"><xsl:value-of select="value"/></xsl:variable>itk::Object* </xsl:when>
  <xsl:otherwise>itk::Object<xsl:text> *</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:value-of select="$var"/>
  <xsl:if test="position() &lt; last()"><xsl:text>, </xsl:text>
  </xsl:if>
</xsl:for-each>) 
{
<xsl:for-each select="/filter/filter-itk/inputs/input">
<xsl:variable name="type"><xsl:value-of select="value"/></xsl:variable>
<xsl:variable name="data">data<xsl:value-of select="@num"/></xsl:variable>
<xsl:variable name="obj">obj<xsl:value-of select="@num"/></xsl:variable>
<xsl:variable name="import"><xsl:value-of select="@import"/></xsl:variable>
<xsl:text>  </xsl:text>
<xsl:choose>
  <xsl:when test="$import = 'yes'">
  <xsl:variable name="import_filter"><xsl:value-of select="value"/></xsl:variable>
  <xsl:variable name="port"><xsl:value-of select="port"/></xsl:variable>
  <xsl:value-of select="/filter/filter-itk/filter[@name=$import_filter]/filter-itk/inputs/input[@num=$port]/value"/>
  </xsl:when>
  <xsl:otherwise>
  <xsl:value-of select="$type"/> *<xsl:value-of select="$data"/> = dynamic_cast&lt;  <xsl:value-of select="$type"/> * &gt;(<xsl:value-of select="$obj"/>);
</xsl:otherwise>
</xsl:choose>
  <xsl:variable name="optional"><xsl:value-of select="@optional"/></xsl:variable>
  <xsl:choose>
  <xsl:when test="$optional = 'yes'">
  if( inport<xsl:value-of select="@num"/>_has_data_ ) {
    if( !<xsl:value-of select="$data"/> ) {
    return false;
    }
  }
  </xsl:when>
  <xsl:otherwise>
  if( !<xsl:value-of select="$data"/> ) {
    return false;
  }
</xsl:otherwise>
</xsl:choose>
</xsl:for-each>
  // create a new filter
  typename <xsl:value-of select="$itk-name"/>&lt; <xsl:for-each select="/filter/filter-itk/templated/template"><xsl:value-of select="."/>
  <xsl:if test="position() &lt; last()">   
  <xsl:text>, </xsl:text>
  </xsl:if>
 </xsl:for-each> &gt;::Pointer filter = <xsl:value-of select="$itk-name"/>&lt; <xsl:for-each select="/filter/filter-itk/templated/template"><xsl:value-of select="."/>
  <xsl:if test="position() &lt; last()">
  <xsl:text>, </xsl:text>
  </xsl:if></xsl:for-each> &gt;::New();

  // set filter 
  <xsl:for-each select="/filter/filter-itk/parameters/param">
  <xsl:variable name="type"><xsl:value-of select="type"/></xsl:variable>
  <xsl:variable name="name"><xsl:value-of select="name"/></xsl:variable>
  <xsl:choose>
  <xsl:when test="$type='bool'">
  <!-- HANDLE CHECKBUTTON -->
  if( gui_<xsl:value-of select="name"/>_.get() ) {
    filter-><xsl:value-of select="call[@value='on']"/>( );   
  } 
  else { 
    filter-><xsl:value-of select="call[@value='off']"/>( );
  }  
  </xsl:when>
  <xsl:otherwise>
  filter-><xsl:value-of select="call"/>( gui_<xsl:value-of select="name"/>_.get() ); 
  </xsl:otherwise>
  </xsl:choose>
  </xsl:for-each>
<xsl:text>   
  // set inputs 
</xsl:text>
  <xsl:for-each select="/filter/filter-itk/inputs/input">
  filter-><xsl:value-of select="call"/>( data<xsl:value-of select="@num"/> );
   </xsl:for-each>

  // execute the filter
  try {

    filter->Update();

  } catch ( itk::ExceptionObject &amp; err ) {
     error("ExceptionObject caught!");
     error(err.GetDescription());
  }

  // get filter output
  <xsl:for-each select="/filter/filter-itk/outputs/output">
  <xsl:variable name="const"><xsl:value-of select="call/@const"/></xsl:variable>
  <xsl:variable name="name"><xsl:value-of select="value"/></xsl:variable>
  <xsl:variable name="output"><!-- hard coded datatype -->ITKDatatype</xsl:variable>
  <xsl:choose>
  <xsl:when test="$const = 'yes'">
  out<xsl:value-of select="@num"/>_->data_ = const_cast&lt;<xsl:value-of select="value"/>*  &gt;(filter-><xsl:value-of select="call"/>());
  </xsl:when>
  <xsl:otherwise>
  out<xsl:value-of select="@num"/>_->data_ = filter-><xsl:value-of select="call"/>();
  </xsl:otherwise>
  </xsl:choose>
  outhandle<xsl:value-of select="@num"/>_ = out<xsl:value-of select="@num"/>_; 
  </xsl:for-each>
  return true;
<xsl:text>}
</xsl:text>
</xsl:template>



<!-- ====== DEFINE_CONSTRUCTOR ====== -->
<xsl:template name="define_constructor">
<xsl:value-of select="$sci-name"/>
<xsl:text>::</xsl:text><xsl:value-of select="$sci-name"/>
<xsl:text disable-output-escaping="yes">(GuiContext* ctx)
  : Module(&quot;</xsl:text>
<xsl:value-of select="$sci-name"/>
<xsl:text disable-output-escaping="yes">&quot;, ctx, Source, &quot;</xsl:text>
<xsl:value-of select="$category"/><xsl:text>&quot;, &quot;</xsl:text>
<xsl:value-of select="$package"/><xsl:text>&quot;)</xsl:text>
<xsl:for-each select="/filter/filter-itk/parameters/param">
<xsl:text>,
     gui_</xsl:text><xsl:value-of select="name"/>
<xsl:text disable-output-escaping="yes">_(ctx->subVar(&quot;</xsl:text>
<xsl:value-of select="name"/>
<xsl:text disable-output-escaping="yes">&quot;))</xsl:text>
</xsl:for-each>
<xsl:text>
{
</xsl:text>
<xsl:for-each select="/filter/filter-itk/inputs/input">
  <xsl:variable name="optional"><xsl:value-of select="@optional"/></xsl:variable>
  <xsl:choose>
  <xsl:when test="$optional = 'yes'">  inport<xsl:value-of select="@num"/>_has_data_ = false;  </xsl:when>
  <xsl:otherwise>
</xsl:otherwise>
</xsl:choose></xsl:for-each>
<xsl:for-each select="/filter/filter-itk/outputs/output">
<xsl:variable name="type-name"><xsl:value-of select="value"/></xsl:variable>
  out<xsl:value-of select="@num"/>_ = scinew <!-- hard coded datatype -->ITKDatatype;</xsl:for-each>
<xsl:text>
}

</xsl:text>
</xsl:template>



<!-- ====== DEFINE_DESTRUCTOR ====== -->
<xsl:template name="define_destructor">
<xsl:value-of select="/filter/filter-sci/@name"/>
<xsl:text>::~</xsl:text><xsl:value-of select="/filter/filter-sci/@name"/>
<xsl:text>() 
{
}

</xsl:text>
</xsl:template>



<!-- ====== DEFINE_EXECUTE ====== -->
<xsl:template name="define_execute">
<xsl:text>void </xsl:text>
<xsl:value-of select="/filter/filter-sci/@name"/>
<xsl:text>::execute() 
{
  // check input ports
</xsl:text>
<xsl:for-each select="/filter/filter-itk/inputs/input">
<xsl:variable name="type-name"><xsl:value-of select="value"/></xsl:variable>
<xsl:variable name="iport">inport<xsl:value-of select="@num"/>_</xsl:variable>
<xsl:variable name="ihandle">inhandle<xsl:value-of select="@num"/>_</xsl:variable>
<xsl:variable name="port-type"><!-- hard coded datatype -->ITKDatatype<xsl:text>IPort</xsl:text></xsl:variable>
<xsl:variable name="optional"><xsl:value-of select="@optional"/></xsl:variable>

<xsl:text>  </xsl:text><xsl:value-of select="$iport"/><xsl:text> = (</xsl:text>
<xsl:value-of select="$port-type"/><xsl:text> *)get_iport(&quot;</xsl:text><xsl:value-of select="name"/><xsl:text>&quot;);
  if(!</xsl:text><xsl:value-of select="$iport"/><xsl:text>) {
    error(&quot;Unable to initialize iport&quot;);
    return;
  }

  </xsl:text>
<xsl:value-of select="$iport"/><xsl:text>->get(</xsl:text>
<xsl:value-of select="$ihandle"/><xsl:text>);
</xsl:text>
<xsl:choose>
  <xsl:when test="$optional = 'yes'">
  if(!<xsl:value-of select="$ihandle"/><xsl:text>.get_rep()) {
    remark("No data in optional </xsl:text><xsl:value-of select="$iport"/>!");			       
    <xsl:value-of select="$iport"/>has_data_ = false;
  }
  else {
    <xsl:value-of select="$iport"/>has_data_ = true;
  }

  </xsl:when>
  <xsl:otherwise>
  if(!<xsl:value-of select="$ihandle"/><xsl:text>.get_rep()) {
    error("No data in </xsl:text><xsl:value-of select="$iport"/><xsl:text>!");			       
    return;
  }

</xsl:text>
</xsl:otherwise>
</xsl:choose>
</xsl:for-each>

<xsl:text>
  // check output ports
</xsl:text>
<xsl:for-each select="/filter/filter-itk/outputs/output">
<xsl:variable name="type-name"><xsl:value-of select="value"/></xsl:variable>
<xsl:variable name="oport">outport<xsl:value-of select="@num"/>_</xsl:variable>
<xsl:variable name="ohandle">outhandle<xsl:value-of select="@num"/>_</xsl:variable>
<xsl:variable name="port-type"><!-- hard coded datatype -->ITKDatatype<xsl:text>OPort</xsl:text></xsl:variable>

<xsl:text>  </xsl:text><xsl:value-of select="$oport"/><xsl:text> = (</xsl:text>
<xsl:value-of select="$port-type"/><xsl:text> *)get_oport(&quot;</xsl:text><xsl:value-of select="name"/><xsl:text>&quot;);
  if(!</xsl:text><xsl:value-of select="$oport"/><xsl:text>) {
    error(&quot;Unable to initialize oport&quot;);
    return;
  }
</xsl:text>
</xsl:for-each>

<xsl:text>
  // get input
  </xsl:text>
<xsl:for-each select="/filter/filter-itk/inputs/input">
<xsl:variable name="ihandle">inhandle<xsl:value-of select="@num"/>_</xsl:variable>
<xsl:variable name="optional"><xsl:value-of select="@optional"/></xsl:variable>
<xsl:choose>
  <xsl:when test="$optional = 'yes'">
  itk::Object<xsl:text>* data</xsl:text><xsl:value-of select="@num"/> = 0;
  if( inport<xsl:value-of select="@num"/>_has_data_ ) {
    data<xsl:value-of select="@num"/> = <xsl:value-of select="$ihandle"/>.get_rep()->data_.GetPointer();
  }
  </xsl:when>
  <xsl:otherwise>itk::Object<xsl:text>* data</xsl:text><xsl:value-of select="@num"/><xsl:text> = </xsl:text><xsl:value-of select="$ihandle"/><xsl:text>.get_rep()->data_.GetPointer();
  </xsl:text>
  </xsl:otherwise>
</xsl:choose>
</xsl:for-each>
<xsl:variable name="defaults"><xsl:value-of select="/filter/filter-sci/instantiations/@use-defaults"/></xsl:variable>
<xsl:text>
  // can we operate on it?
  if(0) { }</xsl:text>
  <xsl:choose>
    <xsl:when test="$defaults = 'yes'">
<xsl:for-each select="/filter/filter-itk/templated/defaults"><xsl:text>
  else if(run&lt; </xsl:text>
  <xsl:for-each select="default">
  <xsl:value-of select="."/><xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if>
    </xsl:for-each>
<xsl:text> &gt;( </xsl:text>
<xsl:for-each select="/filter/filter-itk/inputs/input">data<xsl:value-of select="@num"/>
<xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if></xsl:for-each>
    <xsl:text> )) {} </xsl:text>
</xsl:for-each>
    </xsl:when>
    <xsl:otherwise>
  <xsl:for-each select="/filter/filter-sci/instantiations/instance">
  <xsl:variable name="num"><xsl:value-of select="@num"/></xsl:variable>
  <xsl:text> 
  else if(run&lt; </xsl:text>
<xsl:for-each select="type">
<xsl:variable name="type"><xsl:value-of select="@name"/></xsl:variable>
<xsl:for-each select="/filter/filter-itk/templated/template">
<xsl:variable name="templated_type"><xsl:value-of select="."/></xsl:variable>	      
<xsl:if test="$type = $templated_type">
<xsl:value-of select="/filter/filter-sci/instantiations/instance[@num=$num]/type[@name=$type]/value"/>
<xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if>
</xsl:if>
</xsl:for-each>
</xsl:for-each>
<xsl:text> &gt;( </xsl:text>
<xsl:for-each select="/filter/filter-itk/inputs/input">data<xsl:value-of select="@num"/>
<xsl:if test="position() &lt; last()">
<xsl:text>, </xsl:text>
</xsl:if></xsl:for-each><xsl:text> )) { }</xsl:text></xsl:for-each>
</xsl:otherwise>
</xsl:choose>
<xsl:text>
  else {
    // error
    error(&quot;Incorrect input type&quot;);
    return;
  }

  // send the data downstream
  </xsl:text>
<xsl:for-each select="/filter/filter-itk/outputs/output">
<xsl:variable name="ohandle">outhandle<xsl:value-of select="@num"/>_</xsl:variable>
<xsl:variable name="oport">outport<xsl:value-of select="@num"/>_</xsl:variable>

<xsl:value-of select="$oport"/><xsl:text>->send(</xsl:text><xsl:value-of select="$ohandle"/>
<xsl:text>);
  </xsl:text>
</xsl:for-each>
<xsl:text>
}

</xsl:text>
</xsl:template>



<!-- ====== DEFINE_TCL_COMMAND ====== -->
<xsl:template name="define_tcl_command">
<xsl:text>void </xsl:text>
<xsl:value-of select="/filter/filter-sci/@name"/>
<xsl:text disable-output-escaping="yes">::tcl_command(GuiArgs&amp; args, void* userdata)
{
  Module::tcl_command(args, userdata);
</xsl:text>

<xsl:text>
}

</xsl:text>
</xsl:template>


</xsl:stylesheet>