#!/usr/bin/env python3
#
# PatentClaimVisualizer.py ... Draw the claim hierarchy of a patent
#
# Copyright (c) 2011  KennyTM~ <kennytm@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice, 
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of the KennyTM~ nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

from tkinter import *
from tkinter.ttk import *
from tkinter.messagebox import showerror
from tkinter.scrolledtext import ScrolledText
from lxml import html
from collections import namedtuple
import re
import sys

COMMAND_KEY = 'Command' if sys.platform == 'darwin' else 'Control'

# ref: http://stackoverflow.com/questions/4266566/how-to-show-the-stardand-popup-menu-in-python-tkinter-text-widget-when-mouse-righ
def rClicker(e):
    'right click context menu for all Tk Entry and Text widgets'
    try:
        def rClick_Copy(e, apnd=0):
            e.widget.event_generate('<' + COMMAND_KEY + '-c>')

        def rClick_Cut(e):
            e.widget.event_generate('<' + COMMAND_KEY + '-x>')

        def rClick_Paste(e):
            e.widget.event_generate('<' + COMMAND_KEY + '-v>')

        e.widget.focus()

        nclst=[(' Cut', lambda e=e: rClick_Cut(e)), (' Copy', lambda e=e: rClick_Copy(e)), (' Paste', lambda e=e: rClick_Paste(e))]

        rmenu = Menu(None, tearoff=0, takefocus=0)

        for (txt, cmd) in nclst:
            rmenu.add_command(label=txt, command=cmd)

        rmenu.tk_popup(e.x_root+40, e.y_root+10,entry="0")

    except TclError:
        print(' - rClick menu, something wrong')
        pass

    return "break"


start_of_claim_matcher = re.compile(r'^(\d+)\.').match
claim_type_matcher = re.compile(r'^(\d+)\.\s*(?:An?|The)?\s+(.+?)\s+(?:in the form|operating|according|comprising|adapted|using|that|for|of|as)').match
parent_searcher = re.compile(r'claim\s+(\d+)').search

class ClaimRetrievalError(Exception):
    def __init__(self, msg):
        self.msg = msg
    def __str__(self):
        return self.msg


def get_claims_from_url(url):
    try:
        root = html.parse(url)
    except IOError as e:
        raise ClaimRetrievalError(str(e))
    patent_div = root.find('//div[@id="patent_claims_v"]')
    if patent_div is None:
        raise ClaimRetrievalError("No claims found")
    previous_claim = []
    append_to_claim = previous_claim.append
    for tag in patent_div.iterdescendants():
        claim_text = tag.text
        if claim_text:
            if tag.tag == 'dd':
                claim_text = "  " + claim_text
            elif start_of_claim_matcher(claim_text):
                if previous_claim:
                    yield '\n\n'.join(previous_claim)
                previous_claim = []
                append_to_claim = previous_claim.append
            append_to_claim(claim_text)
    if previous_claim:
        yield '\n'.join(previous_claim)


Claim = namedtuple('Claim', 'index,type,parent,text')
def parse_claims(claims):
    for claim in claims:
        claim_type_match = claim_type_matcher(claim)
        if claim_type_match:
            index = claim_type_match.group(1)
            type_ = claim_type_match.group(2)
        else:
            start_of_claim_match = start_of_claim_matcher(claim)
            index = start_of_claim_match.group(1) if start_of_claim_match else '-'
            type_ = '(N/A)'
        parent_match = parent_searcher(claim)
        parent = parent_match.group(1) if parent_match else ''
        yield Claim(index, type_, parent, claim)


class PCVApp(Frame):
    def __init__(self, master=None):
        super().__init__(master)
        self.pack(fill='both', expand=True)
        self.create_widgets()

    def create_widgets(self):
        url_frame = Frame(self)
        url_frame.pack(anchor='w', fill='x')
        url_label = Label(url_frame, text='Google Patent URL: ')
        url_label.pack(side='left')
        url_entry = Entry(url_frame, width=75)
        url_entry.pack(side='left', expand=True, fill='x')
        url_entry.bind('<Return>', self.analyze_patent)
        url_entry.bind('<2>', rClicker)

        claim_pane = PanedWindow(self, orient=HORIZONTAL)
        claim_pane.pack(anchor='n', fill='both', expand=True)
        claim_treeview = Treeview(claim_pane, columns=('Type',), displaycolumns='#all', selectmode='browse')
        claim_treeview.column('Type', stretch=True)
        claim_treeview.heading('Type', text='Type')
        claim_treeview.heading('#0', text='#')
        claim_treeview.bind('<<TreeviewSelect>>', self.display_claim)
        claim_pane.add(claim_treeview)
        claim_text = ScrolledText(claim_pane, font=('Helvetica', 17), wrap='word')
        claim_text.bind('<2>', rClicker)
        claim_pane.add(claim_text)

        self.claim_treeview = claim_treeview
        self.claim_text = claim_text

    def analyze_patent(self, event):
        claim_treeview = self.claim_treeview
        url = event.widget.get()
        if not url.startswith('http'):
            url = 'http://www.google.com/patents/about?id=' + url
        try:
            claim_treeview.delete(*claim_treeview.get_children())
            for index, type_, parent, text in parse_claims(get_claims_from_url(url)):
                claim_treeview.insert(parent=parent, index='end', iid=index, text=index, values=(type_,), open=True, tags=(text,))

        except ClaimRetrievalError as e:
            showerror('Claim retrieval failed', str(e), parent=self)

    def display_claim(self, event):
        claim_treeview = event.widget
        item = claim_treeview.focus()
        tags = claim_treeview.item(item, 'tags')
        claim_text = self.claim_text
        claim_text.delete("1.0", END)
        claim_text.insert("1.0", tags[0])


root = Tk()
if __name__ == '__main__':
    app = PCVApp(master=root)
    root.title('Patent claim visualizer')
    app.mainloop()

