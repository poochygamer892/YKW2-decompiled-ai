import UIKit
import UniformTypeIdentifiers

/// First-launch menu for importing a user-selected, authorized 3DS image.
/// The NCCH/RomFS extractor is provided by the Objective-C++ bridge.
final class ImportViewController: UIViewController, UIDocumentPickerDelegate {
    private let rightsSwitch = UISwitch()
    private let statusLabel = UILabel()
    private let runtimeButton = UIButton(type: .system)

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = "Yo-kai Watch 2"

        let titleLabel = UILabel()
        titleLabel.text = "Importar una copia autorizada"
        titleLabel.font = .preferredFont(forTextStyle: .title2)
        titleLabel.textAlignment = .center

        let rightsLabel = UILabel()
        rightsLabel.text = "Tengo derecho a usar esta copia en este dispositivo"
        rightsLabel.numberOfLines = 0

        let rightsRow = UIStackView(arrangedSubviews: [rightsLabel, rightsSwitch])
        rightsRow.axis = .horizontal
        rightsRow.alignment = .center
        rightsRow.spacing = 12

        let button = UIButton(type: .system)
        button.setTitle("Seleccionar ROM", for: .normal)
        button.addTarget(self, action: #selector(selectRom), for: .touchUpInside)

        runtimeButton.setTitle("Abrir menú inicial", for: .normal)
        runtimeButton.addTarget(self, action: #selector(openRuntime), for: .touchUpInside)
        runtimeButton.isHidden = true

        statusLabel.text = "Selecciona una copia local autorizada para comenzar."
        statusLabel.numberOfLines = 0
        statusLabel.textAlignment = .center

        let stack = UIStackView(arrangedSubviews: [titleLabel, rightsRow, button, runtimeButton, statusLabel])
        stack.axis = .vertical
        stack.spacing = 20
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24),
            stack.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
        ])

        if let cacheURL = try? FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        ).appendingPathComponent("ykw2-cache", isDirectory: true),
           YKW2ImportBridge.hasReadyImport(at: cacheURL) {
            runtimeButton.isHidden = false
            statusLabel.text = "Recursos importados y verificados. El runtime del juego aún está en desarrollo."
        }
    }

    @objc private func openRuntime() {
        guard let cacheURL = try? FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        ).appendingPathComponent("ykw2-cache", isDirectory: true) else { return }
        let controller = RuntimeViewController(cacheURL: cacheURL)
        if let navigationController {
            navigationController.pushViewController(controller, animated: true)
        } else {
            present(controller, animated: true)
        }
    }

    @objc private func selectRom() {
        guard rightsSwitch.isOn else {
            statusLabel.text = "Confirma primero que tienes derecho a usar la copia."
            return
        }
        let picker = UIDocumentPickerViewController(
            forOpeningContentTypes: [UTType.data],
            asCopy: false
        )
        picker.delegate = self
        picker.allowsMultipleSelection = false
        present(picker, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else {
            statusLabel.text = "No se seleccionó ningún archivo."
            return
        }
        guard url.startAccessingSecurityScopedResource() else {
            statusLabel.text = "No se pudo obtener acceso temporal al archivo."
            return
        }
        do {
            let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
            let size = (attributes[.size] as? NSNumber)?.uint64Value ?? 0
            guard size >= 0x200 else {
                url.stopAccessingSecurityScopedResource()
                statusLabel.text = "La imagen es demasiado pequeña."
                return
            }
            let handle = try FileHandle(forReadingFrom: url)
            defer { try? handle.close() }
            let header = try handle.read(upToCount: 0x200) ?? Data()
            guard header.count == 0x200,
                  header.subdata(in: 0x100..<0x104) == Data("NCSD".utf8) else {
                url.stopAccessingSecurityScopedResource()
                statusLabel.text = "La imagen no es un dump NCSD/3DS válido."
                return
            }
            try? handle.close()
            statusLabel.text = "NCSD válido. Extrayendo recursos en el almacenamiento privado..."
            let cacheURL = try FileManager.default.url(
                for: .applicationSupportDirectory,
                in: .userDomainMask,
                appropriateFor: nil,
                create: true
            ).appendingPathComponent("ykw2-cache", isDirectory: true)
            DispatchQueue.global(qos: .userInitiated).async {
                var importError: NSError?
                let success = YKW2ImportBridge.importDocument(
                    at: url,
                    cacheURL: cacheURL,
                    error: &importError
                )
                DispatchQueue.main.async {
                    url.stopAccessingSecurityScopedResource()
                    if success {
                        self.runtimeButton.isHidden = false
                        self.statusLabel.text = "Importación completada. Recursos guardados en el caché privado."
                    } else {
                        self.statusLabel.text = "No se pudo extraer la RomFS: \(importError?.localizedDescription ?? "error desconocido")"
                    }
                }
            }
        } catch {
            url.stopAccessingSecurityScopedResource()
            statusLabel.text = "No se pudo leer la imagen: \(error.localizedDescription)"
        }
    }
}
