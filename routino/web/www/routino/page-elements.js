//
// Javascript for page elements.
//
// Part of the Routino routing software.
//
// This file Copyright 2008-2012 Andrew M. Bishop
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//


//
// Display one of the tabs and associated DIV and hide the others
//

function tab_select(name)
{
 var tab=document.getElementById("tab_" + name);

 if(tab.className=="tab_selected")
    return;

 // Hide the deslected tabs and DIVs

 var parent=tab.parentNode;
 var child=parent.firstChild;

 do
   {
    if(String(child.id).substr(0,4)=="tab_")
      {
       var div=document.getElementById(child.id + "_div");

       child.className="tab_unselected";
       div.style.display="none";
      }

    child=child.nextSibling;
   }
 while(child!=null);

 // Display the newly selected tab and DIV

 tab.className="tab_selected";
 document.getElementById(tab.id + "_div").style.display="";
}


//
// Show the associated DIV
//

function hideshow_show(name)
{
 document.getElementById("hideshow_" + name + "_show").className="hideshow_hide";
 document.getElementById("hideshow_" + name + "_hide").className="hideshow_show";
 document.getElementById("hideshow_" + name + "_div").style.display="";
}


//
// Hide the associated DIV
//

function hideshow_hide(name)
{
 document.getElementById("hideshow_" + name + "_show").className="hideshow_show";
 document.getElementById("hideshow_" + name + "_hide").className="hideshow_hide";
 document.getElementById("hideshow_" + name + "_div").style.display="none";
}
