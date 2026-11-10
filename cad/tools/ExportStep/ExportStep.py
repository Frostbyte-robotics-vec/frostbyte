# Frostbyte – Batch-Export: alle F3D-Dateien eines Ordners (mit Unterordnern) nach STEP.
# Ausfuehren in Fusion: Utilities > Add-Ins > Scripts and Add-Ins > Script hinzufuegen.
# Quell- und Zielordner werden per Dialog gewaehlt; die Ordnerstruktur wird gespiegelt.
import adsk.core, adsk.fusion, traceback, os


def collect(root):
    files = []
    for folder, _dirs, names in os.walk(root):
        for n in sorted(names):
            if n.lower().endswith('.f3d'):
                files.append(os.path.join(folder, n))
    return sorted(files)


def pick_folder(ui, title):
    dlg = ui.createFolderDialog()
    dlg.title = title
    if dlg.showDialog() != adsk.core.DialogResults.DialogOK:
        return None
    return dlg.folder


def export_one(app, src, dst):
    doc = None
    try:
        doc = app.documents.open(src, False)
        design = adsk.fusion.Design.cast(doc.products.itemByProductType('DesignProductType'))
        if design is None:
            return 'kein Design in der Datei'
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        opts = design.exportManager.createSTEPExportOptions(dst, design.rootComponent)
        if not design.exportManager.execute(opts):
            return 'Export lieferte False'
        return None
    except Exception:
        return traceback.format_exc().strip().splitlines()[-1]
    finally:
        if doc is not None:
            doc.close(False)                   # nie speichern, Quelle bleibt unveraendert


def run(context):
    app = adsk.core.Application.get()
    ui = app.userInterface
    try:
        src_root = pick_folder(ui, 'Quellordner mit F3D-Dateien (z. B. fusion_parts_library_1_0_0)')
        if not src_root:
            return
        dst_root = pick_folder(ui, 'Zielordner fuer die STEP-Dateien')
        if not dst_root:
            return
        files = collect(src_root)
        if not files:
            ui.messageBox('Keine F3D-Dateien in ' + src_root)
            return
        if ui.messageBox('%d F3D-Dateien nach STEP exportieren?\nDas dauert eine Weile.' % len(files),
                         'Frostbyte STEP-Export',
                         adsk.core.MessageBoxButtonTypes.OKCancelButtonType) != adsk.core.DialogResults.DialogOK:
            return

        progress = ui.createProgressDialog()
        progress.isBackgroundTranslucent = False
        progress.show('STEP-Export', 'Datei %v von %m', 0, len(files), 1)
        ok, errors = 0, []
        for i, src in enumerate(files, 1):
            rel = os.path.relpath(src, src_root)
            dst = os.path.join(dst_root, os.path.splitext(rel)[0] + '.step')
            progress.progressValue = i
            if progress.wasCancelled:
                errors.append('abgebrochen vor ' + rel)
                break
            fehler = export_one(app, src, dst)
            if fehler:
                errors.append('%s: %s' % (rel, fehler))
            else:
                ok += 1
            adsk.doEvents()
        progress.hide()

        with open(os.path.join(dst_root, 'export_log.txt'), 'w', encoding='utf-8') as f:
            f.write('Quelle: %s\nExportiert: %d von %d\nFehler: %d\n\n' % (src_root, ok, len(files), len(errors)))
            f.write('\n'.join(errors) + '\n')
        ui.messageBox('Fertig: %d von %d exportiert, %d Fehler.\nProtokoll: export_log.txt im Zielordner.'
                      % (ok, len(files), len(errors)))
    except Exception:
        ui.messageBox('Fehler:\n' + traceback.format_exc())
