import { FrameNode } from "@kit.ArkUI";


export const bindNode: (id: string, node: FrameNode) => void;

export const unbindNode: (id: string) => void;

export const onWillForeground: () => void;

export const onDidForeground: () => void;

export const onWillBackground: () => void;

export const onDidBackground: () => void;