/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

function onInit() {
    initServerType();

}

function initServerType() {
  var serverType = document.getElementById("server.type").value;
  
  var verboseName;
  var index;
  if (serverType == "pop3") {
      verboseName = "POP Mail Server";
      index = 0;
  } else if (serverType == "imap") {
      verboseName = "IMAP Mail Server";
      index = 1;
  } else if (serverType == "nntp") {
      verboseName = "Newsgroup server (NNTP)";
      index = 2;
  } else if (serverType == "none") {
      verboseName = "Local Mail store";
      index = 3;
  }

  if (index != undefined) {
      var deck = document.getElementById("serverdeck");
      deck.setAttribute("index", index);
  }

  var hostname = document.getElementById("server.hostName").value;
  var username = document.getElementById("server.username").value;

  setDivText("servertype.verbose", verboseName);
  setDivText("servername.verbose", hostname);
  setDivText("username.verbose", username);

}

function setDivText(divname, value) {
    var div = document.getElementById(divname);
    if (!div) return;
    if (div.firstChild)
        div.removeChild(div.firstChild);
    div.appendChild(document.createTextNode(value));
}


function openImapAdvanced()
{
    var imapServer = getImapServer();
    window.openDialog("chrome://messenger/content/am-imap-advanced.xul",
                      "_blank",
                      "chrome,modal", imapServer);

    saveServerLocally(imapServer);
}

function getImapServer() {
    var imapServer = new Array;

    var controls = document.controls;

    // boolean prefs, need to do special convertion    
    imapServer.dualUseFolders = (controls["imap.dualUseFolders"].value == "true" ? true : false);
    imapServer.usingSubscription = (controls["imap.usingSubscription"].value == "true" ? true : false);

    // string prefs
    imapServer.personalNamespace = controls["imap.personalNamespace"].value;
    imapServer.publicNamespace = controls["imap.publicNamespace"].value;
    imapServer.otherUsersNamespace = controls["imap.otherUsersNamespace"].value;
    return imapServer;
}

function saveServerLocally(imapServer)
{
    dump("Saving server..\n");
    var controls = document.controls;

    // boolean prefs, JS does the conversion for us
    controls["imap.dualUseFolders"].value = imapServer.dualUseFolders;
    controls["imap.usingSubscription"].value = imapServer.usingSubscription;

    // string prefs
    controls["imap.personalNamespace"].value = imapServer.personalNamespace;
    controls["imap.publicNamespace"].value = imapServer.publicNamespace;
    controls["imap.otherUsersNamespace"].value = imapServer.otherUsersNamespace;

    dump("Done.\n");
}
