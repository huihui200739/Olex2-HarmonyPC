import { DesktopEditorLayoutBudget, desktopEditorLayoutBudget } from '../model/DesktopDocumentLayout';

// Real source draft editing. Text tools never invoke native computation or scripts.
// Applying/saving remain explicit parent transactions with their original guards.
@Component
export struct OriginalDocumentTextEditor {
  @Prop documentId: string = '';
  @Prop source: string = '';
  @Prop name: string = '';
  @Prop savedName: string = '';
  @Prop savedByteCount: number = 0;
  @Prop busy: boolean = false;
  @Prop draftDirty: boolean = false;
  @Prop documentDirty: boolean = false;
  @Prop ready: boolean = false;
  @Prop message: string = '';
  @Prop canUndo: boolean = false;
  @Prop canRedo: boolean = false;
  @Prop findVisible: boolean = false;
  @Prop goToLineVisible: boolean = false;
  @Prop goToLineInput: string = '';
  @Prop query: string = '';
  @Prop replacement: string = '';
  @Prop selectionStart: number = 0;
  @Prop selectionEnd: number = 0;
  @Prop @Watch('requestTextSelection') selectionSequence: number = 0;
  @Prop focusTarget: string = '';
  @Prop @Watch('requestControlFocus') focusSequence: number = 0;
  private sourceController: TextAreaController = new TextAreaController();
  @State private editorWidth: number = 960;
  @State private editorHeight: number = 640;
  private toolsScroller: Scroller = new Scroller();
  private findReplacementTop: number = 0;
  private editorActive: boolean = false;
  private selectionRequestToken: number = 0;
  private focusRequestToken: number = 0;
  onEdit: (text: string) => void = () => {};
  onApply: () => void = () => {};
  onSave: () => void = () => {};
  onSaveAs: () => void = () => {};
  onRevert: () => void = () => {};
  onCloseDocument: () => void = () => {};
  onBack: () => void = () => {};
  onShortcut: (event: KeyEvent) => boolean = () => false;
  onUndo: () => void = () => {};
  onRedo: () => void = () => {};
  onFindVisible: (visible: boolean) => void = () => {};
  onFindNext: () => void = () => {};
  onFindPrevious: () => void = () => {};
  onGoToLineVisible: (visible: boolean) => void = () => {};
  onGoToLineInput: (text: string) => void = () => {};
  onGoToLine: () => void = () => {};
  onQuery: (text: string) => void = () => {};
  onReplacement: (text: string) => void = () => {};
  onReplace: (all: boolean) => void = () => {};
  onSelection: (start: number, end: number) => void = () => {};
  onFocusedControl: (control: string) => void = () => {};
  onToolError: (message: string) => void = () => {};

  aboutToAppear(): void {
    this.editorActive = true;
  }

  aboutToDisappear(): void {
    this.editorActive = false;
    this.selectionRequestToken++; this.focusRequestToken++;
  }

  private dispatchEditorShortcut(event: KeyEvent, control: string): boolean {
    // Key delivery to this exact editable control is current focus evidence.
    // A reused TextArea may keep native focus across a tab switch without
    // another onFocus callback; do not route undo from stale parent telemetry.
    this.onFocusedControl(control);
    return this.onShortcut(event);
  }

  private requestTextSelection(): void {
    const sequence: number = this.selectionSequence;
    const owner: string = this.documentId;
    const source: string = this.source;
    const token: number = ++this.selectionRequestToken;
    // Apply after rendering, never to another tab or a disposed editor. The
    // controller moves/selects only: the parent draft/history owns all edits.
    setTimeout(() => {
      if (!this.editorActive || this.busy || token !== this.selectionRequestToken ||
        owner !== this.documentId || sequence !== this.selectionSequence || source !== this.source) return;
      const start: number = this.selectionStart;
      const end: number = this.selectionEnd;
      if (!Number.isInteger(start) || !Number.isInteger(end) || start < 0 || end < start || end > this.source.length) return;
      try {
        this.getUIContext().getFocusController().requestFocus('olex2-document-editor-source');
        if (start === end) this.sourceController.caretPosition(start);
        else this.sourceController.setTextSelection(start, end, { menuPolicy: MenuPolicy.HIDE });
      } catch (error) { this.onToolError((error as Error).message || '文本选择未完成；草稿未改变。'); }
    }, 0);
  }

  private requestControlFocus(): void {
    const sequence: number = this.focusSequence;
    const owner: string = this.documentId;
    const target: string = this.focusTarget;
    const token: number = ++this.focusRequestToken;
    setTimeout(() => {
      if (!this.editorActive || this.busy || token !== this.focusRequestToken || owner !== this.documentId ||
        sequence !== this.focusSequence || target.length === 0 || target !== this.focusTarget) return;
      if ((target === 'olex2-document-editor-find-query' || target === 'olex2-document-editor-find-replacement') &&
        !this.findVisible) return;
      if (target === 'olex2-document-editor-go-to-line-input' && !this.goToLineVisible) return;
      try {
        if (target === 'olex2-document-editor-find-query') this.revealEditorTool('query');
        if (target === 'olex2-document-editor-find-replacement') this.revealEditorTool('replacement');
        if (target === 'olex2-document-editor-go-to-line-input') this.revealEditorTool('line');
        this.getUIContext().getFocusController().requestFocus(target);
      }
      catch (error) { this.onToolError((error as Error).message || '输入框未能获得焦点。'); }
    }, 0);
  }

  private editorLayout(): DesktopEditorLayoutBudget {
    return desktopEditorLayoutBudget(this.editorWidth, this.editorHeight, this.findVisible || this.goToLineVisible);
  }

  private updateEditorArea(area: Area): void {
    const width: number = Number(area.width);
    const height: number = Number(area.height);
    if (Number.isFinite(width) && width >= 0 && width !== this.editorWidth) this.editorWidth = width;
    if (Number.isFinite(height) && height >= 0 && height !== this.editorHeight) this.editorHeight = height;
  }

  private updateReplacementArea(area: Area): void {
    const top: number = Number(area.position.y);
    if (Number.isFinite(top)) this.findReplacementTop = Math.max(0, top - 12);
  }

  private revealEditorTool(control: string): void {
    const line: boolean = control === 'line' && this.goToLineVisible;
    if (!line && (!this.findVisible || (control !== 'query' && control !== 'replacement'))) return;
    try {
      // Find is the first tools block. Leave a small inset above the focused field.
      this.toolsScroller.scrollTo({ xOffset: 0,
        yOffset: control === 'replacement' ? this.findReplacementTop : 0, animation: false });
    } catch (error) { this.onToolError((error as Error).message || '工具栏定位未完成；草稿未改变。'); }
  }

  @Builder
  private GoToLineTools() {
    Column({ space: 6 }) {
      TextInput({ text: this.goToLineInput, placeholder: '输入实际行号，从 1 开始' })
        .id('olex2-document-editor-go-to-line-input').width('100%').height(34).fontSize(12).maxLength(12)
        .accessibilityText('跳转到实际文本行号，从 1 开始；Enter 定位，Esc 收起。')
        .enabled(!this.busy).onChange((text: string) => { if (text !== this.goToLineInput) this.onGoToLineInput(text); })
        .onKeyPreIme((event: KeyEvent): boolean => this.dispatchEditorShortcut(event, 'line'))
        .onFocus(() => { this.revealEditorTool('line'); this.onFocusedControl('line'); })
        .onBlur(() => this.onFocusedControl('')).onSubmit(() => this.onGoToLine())
      Flex({ wrap: FlexWrap.Wrap }) {
        Button('定位').id('olex2-document-editor-go-to-line-submit').height(30).fontSize(12)
          .margin({ right: 6, bottom: 4 }).enabled(!this.busy && this.goToLineInput.trim().length > 0)
          .onClick(() => this.onGoToLine())
        Button('取消定位').id('olex2-document-editor-go-to-line-close').height(30).fontSize(12)
          .margin({ right: 6, bottom: 4 }).enabled(!this.busy).onClick(() => this.onGoToLineVisible(false))
      }.width('100%')
      Text('Enter 定位，Esc 收起；按实际换行计数，不把窗口自动折行算作新行。仅移动光标，不改正文。')
        .fontSize(11).fontColor('#516A5A').width('100%').maxLines(2)
    }.id('olex2-document-editor-go-to-line-panel').padding(10).width('100%').backgroundColor('#EAF2EC').borderRadius(8)
  }

  @Builder
  private FindTools() {
    Flex({ direction: this.editorLayout().narrow ? FlexDirection.Column : FlexDirection.Row,
      justifyContent: FlexAlign.SpaceBetween, alignItems: ItemAlign.Start }) {
      Column({ space: 6 }) {
        TextInput({ text: this.query, placeholder: '查找原文（区分大小写，非正则）' })
          .id('olex2-document-editor-find-query').width('100%').height(34).fontSize(12).maxLength(4096)
          .enabled(!this.busy).onChange((text: string) => { if (text !== this.query) this.onQuery(text); })
          .onKeyPreIme((event: KeyEvent): boolean => this.dispatchEditorShortcut(event, 'query'))
          .onFocus(() => { this.revealEditorTool('query'); this.onFocusedControl('query'); })
          .onBlur(() => this.onFocusedControl('')).onSubmit(() => this.onFindNext())
        Flex({ wrap: FlexWrap.Wrap }) {
          Button('上一个').id('olex2-document-editor-find-previous').height(30).fontSize(12)
            .margin({ right: 6, bottom: 4 }).enabled(!this.busy && this.query.length > 0).onClick(() => this.onFindPrevious())
          Button('下一个').id('olex2-document-editor-find-next').height(30).fontSize(12)
            .margin({ right: 6, bottom: 4 }).enabled(!this.busy && this.query.length > 0).onClick(() => this.onFindNext())
          Button('关闭查找').id('olex2-document-editor-find-close').height(30).fontSize(12)
            .margin({ right: 6, bottom: 4 }).enabled(!this.busy).onClick(() => this.onFindVisible(false))
        }.width('100%')
      }.width(this.editorLayout().narrow ? '100%' : '49%').margin({ bottom: 6 })
      Column({ space: 6 }) {
        TextInput({ text: this.replacement, placeholder: '替换为（留空即删除匹配原文）' })
          .id('olex2-document-editor-find-replacement').width('100%').height(34).fontSize(12).maxLength(4096)
          .enabled(!this.busy).onChange((text: string) => { if (text !== this.replacement) this.onReplacement(text); })
          .onKeyPreIme((event: KeyEvent): boolean => this.dispatchEditorShortcut(event, 'replacement'))
          .onFocus(() => { this.revealEditorTool('replacement'); this.onFocusedControl('replacement'); })
          .onBlur(() => this.onFocusedControl(''))
        Flex({ wrap: FlexWrap.Wrap }) {
          Button('替换当前').id('olex2-document-editor-replace-current').height(30).fontSize(12)
            .margin({ right: 6, bottom: 4 }).enabled(!this.busy && this.query.length > 0).onClick(() => this.onReplace(false))
          Button('全部替换').id('olex2-document-editor-replace-all').height(30).fontSize(12)
            .margin({ right: 6, bottom: 4 }).enabled(!this.busy && this.query.length > 0).onClick(() => this.onReplace(true))
        }.width('100%')
      }.width(this.editorLayout().narrow ? '100%' : '49%')
        .onAreaChange((_oldArea: Area, area: Area) => this.updateReplacementArea(area))
    }.id('olex2-document-editor-find-panel').padding(10).width('100%').backgroundColor('#EAF2EC').borderRadius(8)
  }

  build() {
    Column({ space: this.editorLayout().gap }) {
      Row({ space: 8 }) {
        Column({ space: 2 }) {
          Text('文档文本编辑').fontSize(this.editorLayout().compact ? 16 : 19)
            .fontWeight(FontWeight.Medium).fontColor('#253D30').width('100%').maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
          Text(this.draftDirty ? '草稿尚未应用' : this.documentDirty ? '已应用，尚未保存' : '与当前文档一致')
            .id('olex2-document-editor-dirty-state').fontSize(11).fontColor('#738078').width('100%').maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
        }.layoutWeight(1).alignItems(HorizontalAlign.Start)
        Button('返回视图').id('olex2-document-editor-back').height(32).fontSize(12).flexShrink(0)
          .enabled(!this.busy).onClick(() => this.onBack())
      }.width('100%').height(this.editorLayout().headerHeight)
      Scroll(this.toolsScroller) {
        Column({ space: this.editorLayout().compact ? 6 : 10 }) {
          if (this.goToLineVisible) {
            this.GoToLineTools()
          }
          if (this.findVisible) {
            this.FindTools()
          }
          Text(this.name).id('olex2-document-editor-name').fontSize(12).fontColor('#516A5A')
            .width('100%').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
          Text(this.savedName.length > 0 ? '上次保存目标：' + this.savedName + ' · 实际保存 ' + this.savedByteCount + ' 字节' :
            '尚无保存目标；首次保存将打开系统另存为选择器。')
            .id('olex2-document-editor-save-target').fontSize(11).fontColor('#516A5A').width('100%')
            .maxLines(this.editorLayout().compact ? 1 : 2).textOverflow({ overflow: TextOverflow.Ellipsis })
            .accessibilityText(this.savedName.length > 0 ? '上次保存目标：' + this.savedName + '，实际保存 ' +
              this.savedByteCount + ' 字节；不代表当前修改已保存。' : '尚无保存目标；首次保存将打开系统另存为选择器。')
          Flex({ wrap: FlexWrap.Wrap }) {
            Button('应用修改').id('olex2-document-editor-apply').height(32).fontSize(12).backgroundColor('#327550')
              .margin({ right: 6, bottom: 6 }).enabled(this.ready && !this.busy && this.draftDirty).onClick(() => this.onApply())
            Button(this.savedName.length > 0 ? '保存' : '保存…').id('olex2-document-editor-save').height(32).fontSize(12)
              .margin({ right: 6, bottom: 6 })
              .accessibilityText(this.savedName.length > 0 ? '保存到上次目标：' + this.savedName + '，Ctrl+S' : '保存…，Ctrl+S，首次选择保存目标')
              .enabled(this.ready && !this.busy && !this.draftDirty).onClick(() => this.onSave())
            Button('另存为…').id('olex2-document-editor-save-as').height(32).fontSize(12).margin({ right: 6, bottom: 6 })
              .accessibilityText('另存为…，每次打开系统选择器选择保存目标')
              .enabled(this.ready && !this.busy && !this.draftDirty).onClick(() => this.onSaveAs())
            Button('撤回未应用修改').id('olex2-document-editor-revert').height(32).fontSize(12).margin({ right: 6, bottom: 6 })
              .enabled(!this.busy && this.draftDirty).onClick(() => this.onRevert())
            Button('关闭文档…').id('olex2-document-editor-close-document').height(32).fontSize(12).margin({ right: 6, bottom: 6 })
              .enabled(!this.busy).onClick(() => this.onCloseDocument())
          }.width('100%')
          Flex({ wrap: FlexWrap.Wrap }) {
            Button('撤销').id('olex2-document-editor-undo').height(30).fontSize(12).margin({ right: 6, bottom: 6 })
              .enabled(this.ready && !this.busy && this.canUndo).onClick(() => this.onUndo())
            Button('重做').id('olex2-document-editor-redo').height(30).fontSize(12).margin({ right: 6, bottom: 6 })
              .enabled(this.ready && !this.busy && this.canRedo).onClick(() => this.onRedo())
            Button(this.findVisible ? '收起查找' : '查找 / 替换').id('olex2-document-editor-find-toggle')
              .height(30).fontSize(12).margin({ right: 6, bottom: 6 })
              .enabled(this.ready && !this.busy).onClick(() => this.onFindVisible(!this.findVisible))
            Button('跳转到行…').id('olex2-document-editor-go-to-line-toggle').height(30).fontSize(12)
              .margin({ right: 6, bottom: 6 }).enabled(this.ready && !this.busy)
              .onClick(() => this.onGoToLineVisible(!this.goToLineVisible))
          }.width('100%')
          Text('Ctrl+Z / Y 撤销重做；Ctrl+F / H 查找替换，F3 / Shift+F3 前后查找；Ctrl+G 跳转到行。')
            .fontSize(11).fontColor('#738078').width('100%').maxLines(2)
          Text('实际源文本，UTF-8 上限 8 MiB；撤销历史仅保留本次会话。应用与保存分别确认；保存可能规范化格式。')
            .fontSize(11).fontColor('#738078').width('100%').maxLines(2)
          Text('Ctrl+S 保存；另存为重新选择目标。未应用草稿阻止保存；Ctrl+W 关闭保留未保存提醒。返回视图保留草稿。')
            .fontSize(11).fontColor('#738078').width('100%').maxLines(2)
        }.id('olex2-document-editor-tools').width('100%').alignItems(HorizontalAlign.Start)
      }.id('olex2-document-editor-tools-scroll').width('100%').height(this.editorLayout().toolsHeight)
        .scrollable(ScrollDirection.Vertical).scrollBar(BarState.On).clip(true)
      TextArea({ text: this.source, placeholder: '当前原文档源文本…', controller: this.sourceController })
        .id('olex2-document-editor-source').width('100%').height(this.editorLayout().textHeight).fontSize(14).fontFamily('monospace')
        .maxLength(8 * 1024 * 1024).backgroundColor('#FFFFFF').enabled(!this.busy).defaultFocus(true)
        .onKeyPreIme((event: KeyEvent): boolean => this.dispatchEditorShortcut(event, 'source'))
        .onFocus(() => this.onFocusedControl('source')).onBlur(() => this.onFocusedControl(''))
        .onTextSelectionChange((start: number, end: number) => this.onSelection(start, end))
        .onChange((text: string) => { if (text !== this.source) this.onEdit(text); })
      Text(this.busy ? '正在处理文档，请稍候…' : this.message).id('olex2-document-editor-message')
        .fontSize(12).fontColor('#516A5A').width('100%').height(this.editorLayout().statusHeight)
        .maxLines(this.editorLayout().compact ? 1 : 2).textOverflow({ overflow: TextOverflow.Ellipsis })
        .accessibilityText(this.busy ? '正在处理文档，请稍候…' : this.message)
    }.id('olex2-document-editor').padding(this.editorLayout().padding).width('100%').height('100%')
      .backgroundColor('#F4F7F3').clip(true).onAreaChange((_oldArea: Area, area: Area) => this.updateEditorArea(area))
  }
}
