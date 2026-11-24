// Underlined keywords (b̲e̲g̲i̲n̲): hide each U+0332 and underline the letter before it,
// since monospace fonts draw U+0332 as a cell of its own.
const vscode = require('vscode');

const MARKS = /([^\s̲])(̲+)/g;

let letter, mark;

function enabled()
{
    return vscode.workspace.getConfiguration('algol60').get('underlineKeywords', true);
}

function update(editor)
{
    if (!editor || editor.document.languageId !== 'algol60')
        return;
    const letters = [], marks = [];
    if (enabled()) {
        const doc = editor.document;
        const text = doc.getText();
        for (const m of text.matchAll(MARKS)) {
            const at = m.index + m[1].length;
            letters.push(new vscode.Range(doc.positionAt(m.index), doc.positionAt(at)));
            marks.push(new vscode.Range(doc.positionAt(at), doc.positionAt(at + m[2].length)));
        }
    }
    editor.setDecorations(letter, letters);
    editor.setDecorations(mark, marks);
}

function updateAll()
{
    vscode.window.visibleTextEditors.forEach(update);
}

function activate(context)
{
    letter = vscode.window.createTextEditorDecorationType({
        textDecoration: 'underline; text-underline-offset: 2px',
    });
    mark = vscode.window.createTextEditorDecorationType({
        textDecoration: 'none; display: none',
    });
    let timer;
    context.subscriptions.push(
        letter, mark,
        vscode.window.onDidChangeVisibleTextEditors(updateAll),
        vscode.workspace.onDidChangeTextDocument(e => {
            clearTimeout(timer);
            timer = setTimeout(() => vscode.window.visibleTextEditors
                .filter(ed => ed.document === e.document).forEach(update), 100);
        }),
        vscode.workspace.onDidChangeConfiguration(e => {
            if (e.affectsConfiguration('algol60'))
                updateAll();
        }));
    updateAll();
}

module.exports = { activate, deactivate() {} };
