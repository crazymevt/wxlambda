"""Generate the bounded wxStyledTextCtrl scalar bindings (not raw SendMsg).

The explicit whitelist is shared by generated native checks, CLOS methods and
reference metadata. Hand-written text/event/compound operations live separately.
"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
# Lisp name, native getter/setter, indexed domain, value domain/type.
PROPERTIES = [
 ('stc-length','GetLength',None,None,'position'),
 ('stc-current-position','GetCurrentPos','SetCurrentPos',None,'position'),
 ('stc-anchor','GetAnchor','SetAnchor',None,'position'),
 ('stc-selection-count','GetSelections',None,None,'integer'),
 ('stc-selection-caret','GetSelectionNCaret','SetSelectionNCaret','selection','position'),
 ('stc-selection-anchor','GetSelectionNAnchor','SetSelectionNAnchor','selection','position'),
 ('stc-main-selection','GetMainSelection','SetMainSelection',None,'selection'),
 ('stc-multiple-selection-p','GetMultipleSelection','SetMultipleSelection',None,'boolean'),
 ('stc-additional-selection-typing-p','GetAdditionalSelectionTyping','SetAdditionalSelectionTyping',None,'boolean'),
 ('stc-selection-mode','GetSelectionMode','SetSelectionMode',None,'mode'),
 ('stc-rectangular-caret','GetRectangularSelectionCaret','SetRectangularSelectionCaret',None,'position'),
 ('stc-rectangular-anchor','GetRectangularSelectionAnchor','SetRectangularSelectionAnchor',None,'position'),
 ('stc-rectangular-caret-virtual-space','GetRectangularSelectionCaretVirtualSpace','SetRectangularSelectionCaretVirtualSpace',None,'nonnegative'),
 ('stc-rectangular-anchor-virtual-space','GetRectangularSelectionAnchorVirtualSpace','SetRectangularSelectionAnchorVirtualSpace',None,'nonnegative'),
 ('stc-virtual-space-options','GetVirtualSpaceOptions','SetVirtualSpaceOptions',None,'virtual'),
 ('stc-can-undo-p','CanUndo',None,None,'boolean'),
 ('stc-can-redo-p','CanRedo',None,None,'boolean'),
 ('stc-modified-p','GetModify',None,None,'boolean'),
 ('stc-read-only-p','GetReadOnly','SetReadOnly',None,'boolean'),
 ('stc-line-count','GetLineCount',None,None,'integer'),
 ('stc-first-visible-line','GetFirstVisibleLine','SetFirstVisibleLine',None,'line'),
 ('stc-x-offset','GetXOffset','SetXOffset',None,'nonnegative'),
 ('stc-mouse-dwell-time','GetMouseDwellTime','SetMouseDwellTime',None,'nonnegative'),
 ('stc-modification-mask','GetModEventMask','SetModEventMask',None,'mask'),
 ('stc-style-at','GetStyleAt',None,'position','integer'),
 ('stc-style-foreground','StyleGetForeground','StyleSetForeground','style','colour'),
 ('stc-style-background','StyleGetBackground','StyleSetBackground','style','colour'),
 ('stc-style-bold-p','StyleGetBold','StyleSetBold','style','boolean'),
 ('stc-style-italic-p','StyleGetItalic','StyleSetItalic','style','boolean'),
 ('stc-style-underline-p','StyleGetUnderline','StyleSetUnderline','style','boolean'),
 ('stc-style-size','StyleGetSize','StyleSetSize','style','font-size'),
 ('stc-indicator-style','IndicatorGetStyle','IndicatorSetStyle','indicator','indicator-style'),
 ('stc-indicator-foreground','IndicatorGetForeground','IndicatorSetForeground','indicator','colour'),
 ('stc-indicator-under-p','IndicatorGetUnder','IndicatorSetUnder','indicator','boolean'),
 ('stc-current-indicator','GetIndicatorCurrent','SetIndicatorCurrent',None,'indicator'),
 ('stc-indicator-value','GetIndicatorValue','SetIndicatorValue',None,'mask'),
 ('stc-margin-type','GetMarginType','SetMarginType','margin','margin-type'),
 ('stc-margin-width','GetMarginWidth','SetMarginWidth','margin','nonnegative'),
 ('stc-margin-mask','GetMarginMask','SetMarginMask','margin','signed'),
 ('stc-margin-sensitive-p','GetMarginSensitive','SetMarginSensitive','margin','boolean'),
 ('stc-fold-level','GetFoldLevel','SetFoldLevel','line','fold'),
 ('stc-fold-expanded-p','GetFoldExpanded','SetFoldExpanded','line','boolean'),
 ('stc-line-visible-p','GetLineVisible',None,'line','boolean'),
 ('stc-fold-parent','GetFoldParent',None,'line','integer'),
 ('stc-autocomplete-active-p','AutoCompActive',None,None,'boolean'),
 ('stc-autocomplete-separator','AutoCompGetSeparator','AutoCompSetSeparator',None,'separator'),
 ('stc-call-tip-active-p','CallTipActive',None,None,'boolean'),
 ('stc-end-styled','GetEndStyled',None,None,'position'),
]
# Name, argument names/domains, native call, optional result expression.
ACTIONS = [
 ('stc-undo',[], 'require(!control->GetReadOnly(),"Read-only styled text"); control->Undo();',None),
 ('stc-redo',[], 'require(!control->GetReadOnly(),"Read-only styled text"); control->Redo();',None),
 ('begin-stc-undo-action',[], 'require(control->undo_depth<64,"Undo nesting exceeds64"); control->BeginUndoAction(); ++control->undo_depth;',None),
 ('end-stc-undo-action',[], 'require(control->undo_depth>0,"No active undo action"); control->EndUndoAction(); --control->undo_depth;',None),
 ('empty-stc-undo-buffer',[], 'require(!control->undo_depth,"End undo action first"); control->EmptyUndoBuffer();',None),
 ('set-stc-save-point',[], 'control->SetSavePoint();',None),
 ('clear-stc-selections',[], 'control->ClearSelections();',None),
 ('add-stc-selection',[('caret','position'),('anchor','position')], '*result=control->AddSelection(a,b);',True),
 ('drop-stc-selection',[('index','selection')], 'require(control->GetSelections()>1,"Cannot drop the only selection"); control->DropSelectionN(a);',None),
 ('set-stc-selection',[('anchor','position'),('caret','position')], 'control->SetSelection(a,b);',None),
 ('start-stc-styling',[('position','position')], 'control->StartStyling(a);',None),
 ('set-stc-styling',[('length','nonnegative'),('style','style')], 'require(control->GetEndStyled()>=0 && control->GetEndStyled()<=control->GetLength() && a<=control->GetLength()-control->GetEndStyled(),"Styling exceeds document"); control->SetStyling(a,b);',None),
 ('clear-stc-styles',[], 'control->StyleClearAll();',None),
 ('fill-stc-indicator-range',[('start','position'),('length','nonnegative')], 'require(a<=control->GetLength()-b,"Indicator range exceeds document"); control->IndicatorFillRange(a,b);',None),
 ('clear-stc-indicator-range',[('start','position'),('length','nonnegative')], 'require(a<=control->GetLength()-b,"Indicator range exceeds document"); control->IndicatorClearRange(a,b);',None),
 ('define-stc-marker',[('marker','marker'),('symbol','marker-symbol')], 'control->MarkerDefine(a,b);',None),
 ('add-stc-marker',[('line','line'),('marker','marker')], '*result=control->MarkerAdd(a,b);',True),
 ('delete-stc-marker',[('line','line'),('marker','marker')], 'control->MarkerDelete(a,b);',None),
 ('delete-all-stc-markers',[('marker','marker')], 'control->MarkerDeleteAll(a);',None),
 ('stc-markers-at-line',[('line','line')], '*result=static_cast<uint32_t>(control->MarkerGet(a));',True),
 ('toggle-stc-fold',[('line','line')], 'control->ToggleFold(a);',None),
 ('show-stc-lines',[('first','line'),('last','line')], 'require(a<=b,"Invalid line range"); control->ShowLines(a,b);',None),
 ('hide-stc-lines',[('first','line'),('last','line')], 'require(a>0 && a<=b,"Cannot hide first line/invalid range"); control->HideLines(a,b);',None),
 ('ensure-stc-line-visible',[('line','line')], 'control->EnsureVisible(a);',None),
 ('stc-line-from-position',[('position','position')], '*result=control->LineFromPosition(a);',True),
 ('stc-position-from-line',[('line','line')], '*result=control->PositionFromLine(a);',True),
 ('stc-line-end-position',[('line','line')], '*result=control->GetLineEndPosition(a);',True),
 ('stc-column-from-position',[('position','position')], '*result=control->GetColumn(a);',True),
 ('stc-position-from-column',[('line','line'),('column','nonnegative')], '*result=control->FindColumn(a,b);',True),
 ('stc-visible-from-document-line',[('line','line')], '*result=control->VisibleFromDocLine(a);',True),
 ('stc-document-from-visible-line',[('line','line')], '*result=control->DocLineFromVisible(a);',True),
 ('scroll-stc-lines',[('columns','scroll'),('lines','scroll')], 'control->LineScroll(a,b);',None),
 ('ensure-stc-caret-visible',[], 'control->EnsureCaretVisible();',None),
 ('cancel-stc-autocomplete',[], 'control->AutoCompCancel();',None),
 ('complete-stc-autocomplete',[], 'require(!control->GetReadOnly(),"Read-only styled text"); control->AutoCompComplete();',None),
 ('cancel-stc-call-tip',[], 'control->CallTipCancel();',None),
 ('clear-stc-document-style',[], 'control->ClearDocumentStyle();',None),
]

def generate():
    native=['// Generated by tools/stc_bindings.py; explicit checked whitelist.']
    lisp=[';;; Generated by tools/stc_bindings.py.', '(in-package #:wxlambda)']
    exports=[]; records=[]
    for i,(name,get,setter,index,domain) in enumerate(PROPERTIES):
        exports.append(name); args='control'+(' index' if index else '')
        native.append(f'case {i}: {{ stc_validate(control,index,"{index or "unused"}"); auto value=control->{get}('+('index' if index else '')+'); *result='+('stc_pack_colour(value)' if domain=='colour' else 'value')+'; break; }')
        lisp+= [f'(defgeneric {name} ({args}))',f'(defmethod {name} ((control styled-text-control){" index" if index else ""})',f'  ({"not (zerop" if domain=="boolean" else "identity"} (stc-native-get control {i} {"index" if index else "0"})'+('))' if domain=='boolean' else ')')+')']
        if setter:
            lisp += [f'(defgeneric (setf {name}) (value {args}))', f'(defmethod (setf {name}) (value (control styled-text-control){" index" if index else ""})',f'  (stc-native-set control {i} {"index" if index else "0"} '+('(if value 1 0)' if domain=='boolean' else 'value')+') value)']
        records.append(dict(name=name,kind='generic-function',arguments=f'STYLED-TEXT-CONTROL'+(f'; INDEX is a native {index} index.' if index else '.'),returns=('Native boolean.' if domain=='boolean' else 'Native integer; colour properties use packed0xRRGGBB.'),description=f'Bounded wxStyledTextCtrl::{get}'+(f'/{setter} property.' if setter else ' query.'),scope='See docs/STYLED-TEXT.md. UTF-8 byte positions, never Lisp string indices. Checked native receiver, index/domain and GUI-thread lifetime; writes defer from callbacks. No raw Scintilla messages or native resource adoption.',wxwidgets=f'wxStyledTextCtrl::{get}'+('/'+setter if setter else '')+', wxWidgets3.2.12.'))
        if setter: records[-1]['setf']=dict(syntax=f'(setf (wxlambda:{name} control'+(' index' if index else '')+') value)',description=f'Write the corresponding native property; domain {domain}. Return supplied value; native synchronous events remain guarded.')
    (ROOT/'native/stc-get-generated.inc').write_text('\n'.join(native)+'\n')
    native=[]
    for i,(name,get,setter,index,domain) in enumerate(PROPERTIES):
        if not setter: continue
        val='stc_unpack_colour(value)' if domain=='colour' else 'static_cast<int>(value)'
        native.append(f'case {i}: stc_validate(control,index,"{index or "unused"}"); stc_validate(control,value,"{domain}"); control->{setter}('+('index,' if index else '')+val+'); break;')
    (ROOT/'native/stc-set-generated.inc').write_text('// Generated by tools/stc_bindings.py.\n'+'\n'.join(native)+'\n')
    native=[]
    for i,(name,arguments,code,result) in enumerate(ACTIONS):
        exports.append(name); names=[a[0] for a in arguments]
        native.append(f'case {i}: '+' '.join(f'stc_validate(control,{"ab"[j]},"{domain}");' for j,(_,domain) in enumerate(arguments))+' '+code+' break;')
        lisp += [f'(defgeneric {name} (control'+''.join(' '+a for a in names)+'))',f'(defmethod {name} ((control styled-text-control)'+''.join(' '+a for a in names)+')',f'  (stc-native-action control {i} '+(names[0] if names else '0')+' '+(names[1] if len(names)>1 else '0')+')'+(')' if result else ' control)')]
        records.append(dict(name=name,kind='generic-function',arguments='STYLED-TEXT-CONTROL'+''.join(f'; {a.upper()} native {d}.' for a,d in arguments),returns='Native integer result.' if result else 'The control.',description='Invoke native '+', '.join(dict.fromkeys(re.findall(r'control->([A-Za-z0-9_]+)\(',code)))+'. Arguments use the checked native domains described in the styled-text guide.',scope='See docs/STYLED-TEXT.md. Checked UTF-8 byte positions/native indices and GUI-thread lifetime. Native synchronous notifications protected. Read-only coordinate/marker queries are callback-safe. Mutations reject event callbacks except same-control StartStyling/SetStyling/SetStyleBytes in the current innermost STYLENEEDED callback; use owner-bound deferred actions otherwise. No editor policy or custom native object adoption.',wxwidgets='wxStyledTextCtrl, wxWidgets3.2.12.'))
    (ROOT/'native/stc-action-generated.inc').write_text('// Generated by tools/stc_bindings.py.\n'+'\n'.join(native)+'\n')
    (ROOT/'lisp/stc-generated.lisp').write_text('\n'.join(lisp)+'\n')
    Path('/private/tmp/wxl-stc-generated-api.json').write_text(json.dumps(records,indent=2)+'\n')
    Path('/private/tmp/wxl-stc-generated-exports.json').write_text(json.dumps(exports))
    print(f'Generated {len(PROPERTIES)} properties/{len(ACTIONS)} operations')
if __name__=='__main__': generate()
