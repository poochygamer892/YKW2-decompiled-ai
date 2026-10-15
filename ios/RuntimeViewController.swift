import AudioToolbox
import UIKit

/// A lightweight scene renderer shared by the portable runtime on iOS.
final class RuntimeSceneCanvasView: UIView {
    var backgroundImage: UIImage?
    private var screen = "title_menu"
    private var playerX: Float = 0
    private var playerY: Float = 0
    private var speaker = ""
    private var dialogueText = ""
    private var enemyID = ""
    private var playerHP = 0
    private var playerMaxHP = 1
    private var enemyHP = 0
    private var enemyMaxHP = 1
    private var sceneHeightPercent: CGFloat = 58
    private var dialogueTopPercent: CGFloat = 55

    func render(screen: String, playerX: Float, playerY: Float, speaker: String,
                dialogueText: String, enemyID: String, playerHP: Int,
                playerMaxHP: Int, enemyHP: Int, enemyMaxHP: Int,
                sceneHeightPercent: CGFloat, dialogueTopPercent: CGFloat) {
        self.screen = screen
        self.playerX = playerX
        self.playerY = playerY
        self.speaker = speaker
        self.dialogueText = dialogueText
        self.enemyID = enemyID
        self.playerHP = playerHP
        self.playerMaxHP = max(1, playerMaxHP)
        self.enemyHP = enemyHP
        self.enemyMaxHP = max(1, enemyMaxHP)
        self.sceneHeightPercent = min(90, max(20, sceneHeightPercent))
        self.dialogueTopPercent = min(90, max(30, dialogueTopPercent))
        setNeedsDisplay()
    }

    override func draw(_ rect: CGRect) {
        UIColor(red: 0.03, green: 0.04, blue: 0.06, alpha: 1).setFill()
        UIRectFill(rect)
        if let image = backgroundImage {
            let target = CGRect(x: 0, y: 0, width: bounds.width,
                               height: bounds.height * sceneHeightPercent / 100)
            let scale = min(target.width / image.size.width, target.height / image.size.height)
            let imageSize = CGSize(width: image.size.width * scale, height: image.size.height * scale)
            let imageRect = CGRect(x: target.midX - imageSize.width / 2,
                                   y: target.midY - imageSize.height / 2,
                                   width: imageSize.width, height: imageSize.height)
            image.draw(in: imageRect)
        }
        switch screen {
        case "world": drawWorld()
        case "dialogue": drawDialogue()
        case "battle": drawBattle()
        default: drawMenuHint()
        }
    }

    private func drawWorld() {
        let top = bounds.height * 0.08
        let bottom = bounds.height * 0.72
        UIColor(red: 0.4, green: 0.75, blue: 0.8, alpha: 0.3).setStroke()
        let cell = max(24, bounds.width / 9)
        for x in stride(from: CGFloat(0), through: bounds.width, by: cell) {
            let path = UIBezierPath()
            path.move(to: CGPoint(x: x, y: top))
            path.addLine(to: CGPoint(x: x, y: bottom))
            path.stroke()
        }
        for y in stride(from: top, through: bottom, by: cell) {
            let path = UIBezierPath()
            path.move(to: CGPoint(x: 0, y: y))
            path.addLine(to: CGPoint(x: bounds.width, y: y))
            path.stroke()
        }
        UIColor(red: 0.26, green: 0.85, blue: 0.75, alpha: 1).setFill()
        let point = CGPoint(x: bounds.midX + CGFloat(playerX) * 8,
                            y: (top + bottom) / 2 + CGFloat(playerY) * 8)
        UIBezierPath(ovalIn: CGRect(x: point.x - 14, y: point.y - 14, width: 28, height: 28)).fill()
        drawText("Exploración", at: CGPoint(x: 16, y: bottom + 8), color: .white, size: 14)
    }

    private func drawDialogue() {
        let panelTop = bounds.height * dialogueTopPercent / 100
        UIColor(red: 0.03, green: 0.05, blue: 0.1, alpha: 0.92).setFill()
        UIBezierPath(roundedRect: CGRect(x: 12, y: panelTop, width: bounds.width - 24,
                                         height: bounds.height - panelTop - 10), cornerRadius: 14).fill()
        drawText(speaker, at: CGPoint(x: 28, y: panelTop + 14), color: UIColor(
            red: 0.42, green: 0.86, blue: 1, alpha: 1), size: 17)
        drawText(dialogueText, at: CGPoint(x: 28, y: panelTop + 47), color: .white, size: 16)
    }

    private func drawBattle() {
        UIColor(red: 0.06, green: 0.1, blue: 0.18, alpha: 0.95).setFill()
        UIBezierPath(roundedRect: bounds.insetBy(dx: 12, dy: 12), cornerRadius: 16).fill()
        drawText("Combate de Yo-kai Watch 2", at: CGPoint(x: 18, y: 28), color: .white, size: 17)
        drawText("Las reglas, encuentros y comandos se cargarán desde el dump autorizado.",
                 at: CGPoint(x: 18, y: 64), color: .white, size: 14)
    }

    private func drawMenuHint() {
        drawText("Selecciona una opción", at: CGPoint(x: 16, y: bounds.height * 0.72), color: .white, size: 15)
    }

    private func drawText(_ text: String, at point: CGPoint, color: UIColor, size: CGFloat) {
        text.draw(at: point, withAttributes: [
            .font: UIFont.systemFont(ofSize: size),
            .foregroundColor: color,
        ])
    }
}

/// Native host for the platform-neutral RuntimeScene.
final class RuntimeViewController: UIViewController {
    private let cacheURL: URL
    private let statusLabel = UILabel()
    private let selectedLabel = UILabel()
    private let commandLabel = UILabel()
    private let sceneView = RuntimeSceneCanvasView()
    private var displayLink: CADisplayLink?
    private var lastAudioSequence: UInt64 = 0

    init(cacheURL: URL) {
        self.cacheURL = cacheURL
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("RuntimeViewController does not use storyboards")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = "Yo-kai Watch 2"
        statusLabel.numberOfLines = 0
        statusLabel.textAlignment = .center
        selectedLabel.font = .preferredFont(forTextStyle: .title2)
        selectedLabel.textAlignment = .center
        commandLabel.numberOfLines = 0
        commandLabel.textAlignment = .center

        let previous = button("Anterior", #selector(movePrevious))
        let next = button("Siguiente", #selector(moveNext))
        let confirm = button("Confirmar / Interactuar", #selector(confirmSelection))
        let up = button("Arriba", #selector(moveUp))
        let down = button("Abajo", #selector(moveDown))
        let left = button("Izquierda", #selector(moveLeft))
        let right = button("Derecha", #selector(moveRight))
        let attack = button("Atacar", #selector(attack))
        let skill = button("Habilidad", #selector(skill))
        let run = button("Huir", #selector(run))
        let back = button("Volver", #selector(moveBack))

        let stack = UIStackView(arrangedSubviews: [statusLabel, sceneView, selectedLabel,
            previous, next, up, down, left, right, confirm, attack, skill, run, back, commandLabel])
        stack.axis = .vertical
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false
        sceneView.heightAnchor.constraint(equalToConstant: 280).isActive = true
        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(stack)
        view.addSubview(scroll)
        NSLayoutConstraint.activate([
            scroll.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
            stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -24),
            stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 12),
            stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -12),
            stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -48),
        ])
        render(YKW2ImportBridge.startRuntime(at: cacheURL))
        renderBackground()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        displayLink?.invalidate()
        displayLink = CADisplayLink(target: self, selector: #selector(advanceFrame(_:)))
        displayLink?.add(to: .main, forMode: .common)
    }

    override func viewWillDisappear(_ animated: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        super.viewWillDisappear(animated)
    }

    private func button(_ title: String, _ action: Selector) -> UIButton {
        let result = UIButton(type: .system)
        result.setTitle(title, for: .normal)
        result.addTarget(self, action: action, for: .touchUpInside)
        return result
    }

    @objc private func advanceFrame(_ displayLink: CADisplayLink) {
        render(YKW2ImportBridge.advanceRuntime(1.0 / 60.0))
    }

    @objc private func movePrevious() { render(YKW2ImportBridge.handleRuntimeInput(1)) }
    @objc private func moveNext() { render(YKW2ImportBridge.handleRuntimeInput(0)) }
    @objc private func confirmSelection() { render(YKW2ImportBridge.handleRuntimeInput(2)) }
    @objc private func moveUp() { render(YKW2ImportBridge.handleRuntimeInput(4)) }
    @objc private func moveDown() { render(YKW2ImportBridge.handleRuntimeInput(5)) }
    @objc private func moveLeft() { render(YKW2ImportBridge.handleRuntimeInput(6)) }
    @objc private func moveRight() { render(YKW2ImportBridge.handleRuntimeInput(7)) }
    @objc private func attack() { render(YKW2ImportBridge.handleRuntimeInput(8)) }
    @objc private func skill() { render(YKW2ImportBridge.handleRuntimeInput(9)) }
    @objc private func run() { render(YKW2ImportBridge.handleRuntimeInput(10)) }
    @objc private func moveBack() { render(YKW2ImportBridge.handleRuntimeInput(3)) }

    private func render(_ response: String) {
        let lines = response.split(separator: "\n", omittingEmptySubsequences: true)
        var selected = 0
        var status = "runtime.title_menu"
        var command = "none"
        var screen = "title_menu"
        var sceneHeightPercent: CGFloat = 58
        var dialogueTopPercent: CGFloat = 55
        var items: [[String]] = []
        var sceneResourceCount = 0
        var worldMap = "mapa pendiente"
        var playerX: Float = 0
        var playerY: Float = 0
        var speaker = ""
        var eventID = ""
        var eventSymbol = ""
        var eventInstructions = 0
        var eventArguments = 0
        var eventDecodedInstructions = 0
        var lineIndex = 0
        var lineCount = 0
        var operationToken = ""
        var operationStatus = "idle"
        var argumentIndex = 0
        var argumentCount = 0
        var enemyID = ""
        var playerHP = 0
        var playerMaxHP = 1
        var enemyHP = 0
        var enemyMaxHP = 1
        var audioSequence: UInt64 = 0
        var audioCue = ""
        for line in lines {
            let parts = line.split(separator: "=", maxSplits: 1).map(String.init)
            guard parts.count == 2 else { continue }
            switch parts[0] {
            case "screen": screen = parts[1]
            case "selected": selected = Int(parts[1]) ?? 0
            case "status": status = parts[1]
            case "presentation_scene_height_percent": sceneHeightPercent = CGFloat(Double(parts[1]) ?? 58)
            case "presentation_dialogue_top_percent": dialogueTopPercent = CGFloat(Double(parts[1]) ?? 55)
            case "command": command = parts[1]
            case "item": items.append(parts[1].split(separator: "|").map(String.init))
            case "scene_resource": sceneResourceCount += 1
            case "world_map": worldMap = parts[1]
            case "player_x": playerX = Float(parts[1]) ?? 0
            case "player_y": playerY = Float(parts[1]) ?? 0
            case "speaker": speaker = parts[1]
            case "event_id": eventID = parts[1]
            case "event_symbol": eventSymbol = parts[1]
            case "event_instructions": eventInstructions = Int(parts[1]) ?? 0
            case "event_arguments": eventArguments = Int(parts[1]) ?? 0
            case "event_decoded_instructions": eventDecodedInstructions = Int(parts[1]) ?? 0
            case "line_index": lineIndex = Int(parts[1]) ?? 0
            case "line_count": lineCount = Int(parts[1]) ?? 0
            case "event_exec_operation": operationToken = parts[1]
            case "event_exec_operation_status": operationStatus = parts[1]
            case "event_exec_argument_index": argumentIndex = Int(parts[1]) ?? 0
            case "event_exec_argument_count": argumentCount = Int(parts[1]) ?? 0
            case "enemy_id": enemyID = parts[1]
            case "player_hp": playerHP = Int(parts[1]) ?? 0
            case "player_max_hp": playerMaxHP = Int(parts[1]) ?? 1
            case "enemy_hp": enemyHP = Int(parts[1]) ?? 0
            case "enemy_max_hp": enemyMaxHP = Int(parts[1]) ?? 1
            case "audio_sequence": audioSequence = UInt64(parts[1]) ?? 0
            case "audio_cue": audioCue = parts[1]
            default: break
            }
        }
        statusLabel.text = statusText(for: status, screen: screen)
        if items.indices.contains(selected), items[selected].count > 1 {
            selectedLabel.text = label(for: items[selected][1])
        } else if screen == "dialogue" {
            selectedLabel.text = "Pulsa Confirmar para continuar"
        } else if screen == "battle" {
            selectedLabel.text = "Elige una acción"
        } else {
            selectedLabel.text = statusText(for: status, screen: screen)
        }
        let dialogue = eventDescription(eventID: eventID, symbol: eventSymbol,
                                        instructions: eventInstructions, arguments: eventArguments,
                                        decodedInstructions: eventDecodedInstructions,
                                        lineIndex: lineIndex, lineCount: lineCount,
                                        operationToken: operationToken,
                                        operationStatus: operationStatus,
                                        argumentIndex: argumentIndex, argumentCount: argumentCount)
        sceneView.render(screen: screen, playerX: playerX, playerY: playerY, speaker: speaker,
                         dialogueText: dialogue, enemyID: enemyID, playerHP: playerHP,
                         playerMaxHP: playerMaxHP, enemyHP: enemyHP, enemyMaxHP: enemyMaxHP,
                         sceneHeightPercent: sceneHeightPercent,
                         dialogueTopPercent: dialogueTopPercent)
        commandLabel.text = commandText(command: command, screen: screen, speaker: speaker,
                                        dialogue: dialogue, worldMap: worldMap, playerX: playerX,
                                        playerY: playerY, sceneResourceCount: sceneResourceCount,
                                        enemyID: enemyID, enemyHP: enemyHP, enemyMaxHP: enemyMaxHP)
        playAudioCue(sequence: audioSequence, cue: audioCue)
    }

    private func commandText(command: String, screen: String, speaker: String, dialogue: String,
                             worldMap: String, playerX: Float, playerY: Float,
                             sceneResourceCount: Int, enemyID: String, enemyHP: Int,
                             enemyMaxHP: Int) -> String {
        if screen == "dialogue" { return "\(speaker)\n\(dialogue)" }
        if screen == "battle" { return "El intérprete de reglas y encuentros de YW2 aún no está disponible." }
        switch command {
        case "request_authorized_import": return "Selecciona una copia autorizada desde el menú de importación."
        case "start_new_game": return "Nueva partida iniciada."
        case "advance_dialogue": return "Historia avanzada."
        case "start_battle": return "El motor de combate de YW2 todavía no está disponible."
        case "battle_won": return "¡Victoria!"
        case "battle_escaped": return "Has escapado del combate."
        case "battle_defeated": return "Has sido derrotado."
        case "open_settings": return "Las opciones se conectarán en la siguiente fase."
        default: break
        }
        if screen == "world" {
            return "Mapa: \(worldMap)\nJugador: (\(playerX), \(playerY))\n\(sceneResourceCount) recursos cargados. Pulsa Confirmar para combatir."
        }
        return ""
    }

    private func playAudioCue(sequence: UInt64, cue: String) {
        guard sequence > lastAudioSequence else { return }
        lastAudioSequence = sequence
        let sound: SystemSoundID = cue == "victory" ? 1057 : cue == "defeat" ? 1073 : 1104
        AudioServicesPlaySystemSound(sound)
    }

    private func renderBackground() {
        let width = Int(YKW2ImportBridge.runtimeBackgroundWidth())
        let height = Int(YKW2ImportBridge.runtimeBackgroundHeight())
        let data = YKW2ImportBridge.runtimeBackgroundRGBA()
        guard width > 0, height > 0, data.count == width * height * 4,
              let provider = CGDataProvider(data: data as CFData),
              let image = CGImage(width: width, height: height, bitsPerComponent: 8,
                                  bitsPerPixel: 32, bytesPerRow: width * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue |
                                      CGBitmapInfo.byteOrder32Big.rawValue), provider: provider,
                                  decode: nil, shouldInterpolate: false, intent: .defaultIntent) else {
            return
        }
        sceneView.backgroundImage = UIImage(cgImage: image)
        sceneView.setNeedsDisplay()
    }

    private func eventDescription(eventID: String, symbol: String, instructions: Int,
                                  arguments: Int, decodedInstructions: Int,
                                  lineIndex: Int, lineCount: Int, operationToken: String,
                                  operationStatus: String,
                                  argumentIndex: Int, argumentCount: Int) -> String {
        if eventID.isEmpty { return "No se encontró un evento de apertura compatible en el dump." }
        let entry = symbol.isEmpty ? "sin símbolo recuperado" : symbol
        let total = lineCount > 0 ? lineCount : instructions
        let step = total > 0 ? "Paso \(lineIndex + 1)/\(total)" : "Programa sin pasos"
        let operation = operationToken.isEmpty ? "sin operación actual" : "token \(operationToken) · argumentos \(argumentIndex)+\(argumentCount) · \(operationStatus)"
        return "Programa XQ32 de YW2 decodificado\n\(eventID)\nEntrada: \(entry) · \(decodedInstructions)/\(instructions) instrucciones · \(arguments) argumentos\n\(step) · \(operation)\nLa semántica de los tokens sigue en recuperación."
    }

    private func label(for key: String) -> String {
        switch key {
        case "menu.new_game": return "Nueva partida"
        case "menu.continue": return "Continuar"
        case "menu.settings": return "Opciones"
        default: return key
        }
    }

    private func statusText(for key: String, screen: String) -> String {
        if screen == "dialogue" { return "Historia" }
        if screen == "battle" { return "Combate" }
        switch key {
        case "runtime.title_menu": return "Contenido inicial validado"
        case "runtime.world": return "Explorando Yo-kai Watch 2"
        case "runtime.content_missing": return "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        case "import.required": return "Selecciona una copia autorizada para importar."
        case "import.incomplete": return "La importación está incompleta; vuelve a seleccionar el dump."
        default: return key
        }
    }
}
