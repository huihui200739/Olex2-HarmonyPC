import { KeyCode } from '@kit.InputKit';
import { resolveDesktopTabNavigation, desktopTabFocusTarget } from '../model/DesktopTabNavigation';
import { OriginalDocumentSessionSummary } from '../model/OriginalDocumentSessions';
import { OriginalDocumentSaveTarget } from '../model/OriginalDocumentSaveTargets';
import { desktopDocumentTabOffset, DESKTOP_DOCUMENT_TAB_WIDTH, DESKTOP_DOCUMENT_TAB_GAP,
  DESKTOP_DOCUMENT_TAB_PADDING } from '../model/DesktopDocumentLayout';

// Metadata-only strip. The parent owns activation/restore and close transactions.
@Component
export default struct OriginalDocumentTabs {
  @Prop @Watch('activeTabChanged') sessions: OriginalDocumentSessionSummary[] = [];
  @Prop saveTargets: OriginalDocumentSaveTarget[] = [];
  @Prop @Watch('tabAvailabilityChanged') busy: boolean = false;
  @State private keyboardMenuId: string = '';
  private focusedTabId: string = '';
  private tabMetaKeys: Set<number> = new Set<number>();
  onActivate: (id: string) => void = (_id: string): void => {};
  onClose: (id: string) => void = (_id: string): void => {};
  private tabScroller: Scroller = new Scroller();
  private tabViewportWidth: number = 0;
  private tabRevealTimer: number = -1;
  private tabStripAlive: boolean = false;
  private tabRevealPending: boolean = false;
  private lastActiveTabId: string = '';
  private lastTabCount: number = 0;

  aboutToAppear(): void {
    this.tabStripAlive = true;
    this.activeTabChanged();
    this.queueActiveTabReveal();
  }

  aboutToDisappear(): void {
    this.tabStripAlive = false;
    if (this.tabRevealTimer >= 0) clearTimeout(this.tabRevealTimer);
    this.tabRevealTimer = -1;
    this.tabRevealPending = false;
    this.tabViewportWidth = 0;
    this.lastActiveTabId = '';
    this.lastTabCount = 0;
    this.keyboardMenuId = '';
    this.focusedTabId = '';
    this.tabMetaKeys.clear();
  }

  private activeTabId(): string {
    const rows: OriginalDocumentSessionSummary[] = this.sessions.slice(0, 4);
    const active: OriginalDocumentSessionSummary[] = rows.filter(
      (session: OriginalDocumentSessionSummary): boolean => session.active);
    if (active.length !== 1 || active[0].id.length === 0) return '';
    const id: string = active[0].id;
    return rows.filter((session: OriginalDocumentSessionSummary): boolean => session.id === id).length === 1 ? id : '';
  }

  private activeTabChanged(): void {
    this.tabAvailabilityChanged();
    const id: string = this.activeTabId();
    const count: number = this.sessions.slice(0, 4).length;
    // Draft/save metadata refreshes must not undo the user's manual scrolling.
    if (id === this.lastActiveTabId && count === this.lastTabCount) return;
    this.lastActiveTabId = id;
    this.lastTabCount = count;
    this.tabRevealPending = this.tabRevealTargetId().length > 0;
    this.queueActiveTabReveal();
  }

  private measureTabViewport(area: Area): void {
    const measured: number = Number(area.width);
    const width: number = Number.isFinite(measured) ? Math.max(0, measured - DESKTOP_DOCUMENT_TAB_PADDING * 2) : 0;
    if (width !== this.tabViewportWidth) {
      this.tabViewportWidth = width;
      this.tabRevealPending = this.tabRevealTargetId().length > 0;
    }
    this.queueActiveTabReveal();
  }

  private queueActiveTabReveal(): void {
    if (!this.tabStripAlive || !this.tabRevealPending || this.tabViewportWidth <= 0 || this.tabRevealTimer >= 0) return;
    // Reveal the current keyboard receiver, or the active tab if focus left.
    // This only scrolls; no delayed callback requests focus or activates a document.
    this.tabRevealTimer = setTimeout(() => {
      this.tabRevealTimer = -1;
      if (!this.tabStripAlive || !this.tabRevealPending || this.tabViewportWidth <= 0) return;
      const id: string = this.tabRevealTargetId();
      const rows: OriginalDocumentSessionSummary[] = this.sessions.slice(0, 4);
      const index: number = rows.findIndex((session: OriginalDocumentSessionSummary): boolean => session.id === id);
      if (id.length === 0 || index < 0) { this.tabRevealPending = false; return; }
      try {
        const current: number = this.tabScroller.currentOffset().xOffset;
        const target: number | undefined = desktopDocumentTabOffset(index, rows.length, this.tabViewportWidth, current);
        if (target === undefined) return;
        if (!Number.isFinite(current) || Math.abs(target - current) > 0.5) {
          this.tabScroller.scrollTo({ xOffset: target, yOffset: 0, animation: false });
        }
        this.tabRevealPending = false;
      } catch (_error) {
        // Not yet attached: retain pending intent for the next area callback.
        // Never activate/close a document or steal input focus to reveal a tab.
      }
    }, 0);
  }


  private currentSession(session: OriginalDocumentSessionSummary): OriginalDocumentSessionSummary {
    // ForEach keeps stable ids: always read the latest @Prop snapshot, not its
    // item-builder closure's old object when the parent replaces summary rows.
    const current: OriginalDocumentSessionSummary | undefined = this.sessions.find(
      (candidate: OriginalDocumentSessionSummary): boolean => candidate.id === session.id);
    return current === undefined ? session : current;
  }

  private isActive(session: OriginalDocumentSessionSummary): boolean {
    return this.currentSession(session).active;
  }

  private isDirty(session: OriginalDocumentSessionSummary): boolean {
    const current: OriginalDocumentSessionSummary = this.currentSession(session);
    return current.draftDirty || current.documentDirty;
  }

  private sourceName(session: OriginalDocumentSessionSummary): string {
    const current: OriginalDocumentSessionSummary = this.currentSession(session);
    return current.name.length > 0 ? current.name : '未命名源文件';
  }

  private saveTarget(session: OriginalDocumentSessionSummary): OriginalDocumentSaveTarget | undefined {
    // Resolve by stable id on every render/accessibility read. Never capture a
    // target object inside ForEach: a save/Save As replaces the parent snapshot.
    return this.saveTargets.find(
      (candidate: OriginalDocumentSaveTarget): boolean => candidate.id === session.id);
  }

  private displayName(session: OriginalDocumentSessionSummary): string {
    const target: OriginalDocumentSaveTarget | undefined = this.saveTarget(session);
    return target !== undefined && target.name.length > 0 ? target.name : this.sourceName(session);
  }

  private identityLabel(session: OriginalDocumentSessionSummary): string {
    const target: OriginalDocumentSaveTarget | undefined = this.saveTarget(session);
    return '保存目标：' + (target === undefined ? '未选择' : target.name) +
      ' / 原生来源：' + this.sourceName(session);
  }

  private dirtyLabel(session: OriginalDocumentSessionSummary): string {
    const current: OriginalDocumentSessionSummary = this.currentSession(session);
    if (current.draftDirty && current.documentDirty) return '● 待应用·未保存';
    if (current.draftDirty) return '● 待应用';
    if (current.documentDirty) return '● 未保存';
    return '';
  }

  private bindingLabel(session: OriginalDocumentSessionSummary): string {
    // Selection is not proof that this session is bound to the native document.
    const current: OriginalDocumentSessionSummary = this.currentSession(session);
    const selection: string = current.active ? '已选中' : '未选中';
    return selection + (current.nativeBound ? ' · 原生已绑定' : ' · 待恢复');
  }

  private activateLabel(session: OriginalDocumentSessionSummary): string {
    const current: OriginalDocumentSessionSummary = this.currentSession(session);
    return '切换到 ' + this.displayName(current) + '，' + this.identityLabel(current) +
      '，' + this.bindingLabel(current) +
      (current.draftDirty ? '，草稿待应用' : '') + (current.documentDirty ? '，文档未保存' : '');
  }

  private hasSession(id: string): boolean {
    return id.length > 0 && this.sessions.slice(0, 4).filter(
      (session: OriginalDocumentSessionSummary): boolean => session.id === id).length === 1;
  }

  private tabAvailabilityChanged(): void {
    if (this.busy || !this.hasSession(this.keyboardMenuId)) this.keyboardMenuId = '';
    if (this.busy || !this.hasSession(this.focusedTabId)) this.focusedTabId = '';
    if (this.busy) this.tabMetaKeys.clear();
  }

  private tabRevealTargetId(): string {
    return this.hasSession(this.focusedTabId) ? this.focusedTabId : this.activeTabId();
  }

  private tabFocused(id: string): void {
    if (!this.tabStripAlive || this.busy || !this.hasSession(id)) return;
    this.focusedTabId = id;
    this.tabRevealPending = true;
    this.queueActiveTabReveal();
  }

  private tabBlurred(id: string): void {
    if (this.focusedTabId === id) this.focusedTabId = '';
    this.tabMetaKeys.clear();
  }

  private tabKey(event: KeyEvent, id: string): boolean {
    if (event.keyCode === KeyCode.KEYCODE_META_LEFT || event.keyCode === KeyCode.KEYCODE_META_RIGHT) {
      if (event.type === KeyType.Down) this.tabMetaKeys.add(event.keyCode);
      else if (event.type === KeyType.Up) this.tabMetaKeys.delete(event.keyCode);
      return false;
    }
    // A displayed menu owns its navigation, Enter and Escape. Never consume
    // ordinary Tab, typing, clipboard keys or global document shortcuts here.
    if (!this.tabStripAlive || this.busy || !this.hasSession(id) || this.keyboardMenuId.length > 0 ||
      event.type !== KeyType.Down || !event.getModifierKeyState) return false;
    const action: string = resolveDesktopTabNavigation(event.keyCode,
      event.getModifierKeyState(['ctrl']), event.getModifierKeyState(['alt']),
      event.getModifierKeyState(['shift']), this.tabMetaKeys.size > 0);
    if (action.length === 0) return false;
    if (action === 'context') {
      this.keyboardMenuId = id;
    } else {
      const ids: string[] = this.sessions.slice(0, 4).map(
        (session: OriginalDocumentSessionSummary): string => session.id);
      const target: string = desktopTabFocusTarget(ids, id, action);
      if (target.length === 0) return false;
      try {
        // Manual activation: arrows move focus only; Enter/Space retain the
        // Button's existing activation and the parent's unsaved-data checks.
        this.getUIContext().getFocusController().requestFocus(`olex2-document-tab-${target}`);
      } catch (_error) { return false; }
    }
    event.stopPropagation(); return true;
  }

  private keyboardMenuDismissed(id: string): void {
    // A late disappearing popup must not dismiss a different tab's new menu.
    if (this.keyboardMenuId === id) this.keyboardMenuId = '';
  }

  private tabMenuAction(id: string, close: boolean): void {
    if (!this.tabStripAlive || this.busy || !this.hasSession(id)) return;
    this.keyboardMenuId = '';
    if (close) this.onClose(id);
    else this.onActivate(id);
  }

  @Builder
  private TabContextMenu(id: string) {
    Menu() {
      MenuItem({ content: '切换到此文档' }).id(`olex2-tab-context-activate-${id}`)
        .enabled(!this.busy && this.hasSession(id))
        .onClick(() => this.tabMenuAction(id, false))
      MenuItem({ content: '关闭此文档…（未保存时提示）' }).id(`olex2-tab-context-close-${id}`)
        .enabled(!this.busy && this.hasSession(id))
        .onClick(() => this.tabMenuAction(id, true))
    }
  }

  build() {
    if (this.sessions.length > 0) {
      Scroll(this.tabScroller) {
        // Keep the content Row at its intrinsic width so narrow windows scroll.
        Row({ space: DESKTOP_DOCUMENT_TAB_GAP }) {
          ForEach(this.sessions.slice(0, 4), (session: OriginalDocumentSessionSummary) => {
            Row({ space: 0 }) {
              Button({ type: ButtonType.Normal }) {
                Column({ space: 2 }) {
                  Row({ space: 4 }) {
                    Text(this.displayName(session))
                      .fontSize(12).fontWeight(this.isActive(session) ? FontWeight.Medium : FontWeight.Normal)
                      .fontColor('#000000').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
                      .layoutWeight(1)
                    if (this.isDirty(session)) {
                      Text(this.dirtyLabel(session)).fontSize(10).fontColor('#8A4B00')
                        .maxLines(1).flexShrink(0)
                    }
                  }.width('100%')
                  Text(this.identityLabel(session) + ' · ' + this.bindingLabel(session)).fontSize(10).fontColor('#333333')
                    .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).width('100%').textAlign(TextAlign.Start)
                    .accessibilityText(this.identityLabel(session) + '，' + this.bindingLabel(session))
                }.width('100%').alignItems(HorizontalAlign.Start)
              }
              .id(`olex2-document-tab-${session.id}`)
              .width(204).height(44).padding({ left: 10, right: 10, top: 4, bottom: 4 })
              .backgroundColor(Color.Transparent).borderRadius(0)
              .accessibilityText(this.activateLabel(session)).enabled(!this.busy)
              .accessibilityDescription('左右方向键移动焦点，Home / End 到首尾；Enter / 空格切换文档；菜单键或 Shift+F10 打开此标签的操作菜单。')
              .onFocus(() => this.tabFocused(session.id))
              .onBlur(() => this.tabBlurred(session.id))
              .onKeyEvent((event: KeyEvent): boolean => this.tabKey(event, session.id))
              .onClick((): void => {
                if (!this.busy) this.onActivate(session.id);
              })

              // Sibling of the activation Button, never nested in its hit area.
              Button('×', { type: ButtonType.Normal })
                .id(`olex2-document-tab-close-${session.id}`)
                .width(30).height(44).padding(0).fontSize(18).fontColor('#333333')
                .backgroundColor(Color.Transparent).borderRadius(0)
                .accessibilityText('关闭 ' + this.displayName(session) + '，' + this.identityLabel(session)).enabled(!this.busy)
                .onFocus(() => this.tabFocused(session.id))
                .onBlur(() => this.tabBlurred(session.id))
                .onKeyEvent((event: KeyEvent): boolean => this.tabKey(event, session.id))
                .onClick((): void => {
                  if (!this.busy) this.onClose(session.id);
                })
            }
            .width(DESKTOP_DOCUMENT_TAB_WIDTH).height(46).flexShrink(0)
            .backgroundColor(this.isActive(session) ? '#FFFFFF' : '#E8E8E8')
            .border({ width: 1, color: this.isActive(session) ? '#327550' : '#DCE6DE' })
            .borderRadius(6).clip(true)
            .bindContextMenu(() => { this.TabContextMenu(session.id); }, ResponseType.RightClick)
            .bindMenu(this.keyboardMenuId === session.id, () => { this.TabContextMenu(session.id); },
              { onDisappear: () => this.keyboardMenuDismissed(session.id) })
          }, (session: OriginalDocumentSessionSummary): string => session.id)
        }.height(46).onAreaChange(() => this.queueActiveTabReveal())
      }
      .id('olex2-document-tabs').width('100%').height(58)
      .padding({ left: DESKTOP_DOCUMENT_TAB_PADDING, right: DESKTOP_DOCUMENT_TAB_PADDING, top: 6, bottom: 6 })
      .onAreaChange((_oldArea: Area, area: Area) => this.measureTabViewport(area))
      .scrollable(ScrollDirection.Horizontal).scrollBar(BarState.Auto).align(Alignment.Start)
      .backgroundColor('#F0F0F0').enabled(!this.busy).opacity(this.busy ? 0.65 : 1)
    }
  }
}
